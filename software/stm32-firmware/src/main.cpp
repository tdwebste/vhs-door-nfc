#include <mbed.h>
#include <Timer.h>
#include <platform/CircularBuffer.h>

// #include <PN532.h>
// #include <PN532_HSU.h>
// #include <PN532_SPI.h>
// #include <PN532_I2C.h>

#include <MFRC522.h>

// #include <Buzzer.h>
// #include <Tones.h>

#include <PwmSound.h>

#include <Keypad.h>


#define ARRAY_COUNT(arr)  (sizeof(arr) / (sizeof((arr)[0])))


DigitalOut ledNFC(PC_13); // status led

DigitalOut doorRelay(PA_0); // status led


// DigitalOut rstNFC(PB_12); // pn532 chip reset control

// PwmOut buz(PB_1);
// Buzzer buzzer(PB_1);
PwmSound audioPlayback(PB_1);


static char Keytable[] = {
  '1', '2', '3',   // r0
  '4', '5', '6',   // r1
  '7', '8', '9',   // r2
  '*', '0', '#',   // r3
};
// c0   c1   c2

static Keypad keypad(PB_0, PA_7, PA_6, PA_5, // rows
                     PA_4, PA_3, PA_2, 200);  // columns

static char pinCode[16] = {};
static bool pinCompleted = false;
static Timer pinTimeout;

Serial pc(PA_9, PA_10, 115200);

RawSerial esp32(PB_10, PB_11, 115200);
uint8_t esp32RXBuffer[64] = {};
int esp32RXLen = 0;

enum ControlCmd {
  CCMD_NOP,
  CCMD_PLAY_BEEP_01,
  CCMD_PLAY_BEEP_02,
  CCMD_PLAY_BEEP_03,
  CCMD_PLAY_BEEP_04,
  CCMD_PLAY_BEEP_05,
  CCMD_PLAY_BEEP_06,
  CCMD_PLAY_BUZZER_01,
  CCMD_PLAY_BUZZER_02,
  CCMD_PLAY_SUCCESS,
  CCMD_PLAY_FAILURE,
  CCMD_PLAY_SMB,
  CCMD_LOCK_DOOR,
  CCMD_UNLOCK_DOOR
};

CircularBuffer<ControlCmd, 16, uint8_t>  commandBuffer;

// ----------------------------------------- HSU
//HardwareSerial pn532_hsu (PC_10, PC_11);
//PN532_HSU pn532_if (pn532_hsu);

// ----------------------------------------- SPI
// SPI pn532_spi(PB_15, PB_14, PB_13);
// PN532_SPI pn532_if(pn532_spi, PA_8);

// ----------------------------------------- I2C
//I2C pn532_i2c (I2C_SDA, I2C_SCL);
//PN532_I2C pn532_if (pn532_i2c);

// PN532 nfc(pn532_if);

#define SPI2_MOSI       PB_15
#define SPI2_MISO       PB_14
#define SPI2_SCK        PB_13
#define MFRC522_SS_PIN  PB_12
#define MFRC522_RST_PIN PA_8

MFRC522 rfid(SPI2_MOSI, SPI2_MISO, SPI2_SCK, MFRC522_SS_PIN, MFRC522_RST_PIN);

// MFRC522::MIFARE_Key key; 

// uint8_t nuidPICC[4];

// uint8_t valid_card[] = { 0x04, 0x50, 0x8C, 0xEA, 0x50, 0x49, 0x80 };

// bool compareCards(const uint8_t* uid0, const uint8_t len0, const uint8_t* uid1, const uint8_t len1) {
//   if (len0 != len1) {
//     return false;
//   }

//   for (uint8_t i=0; i<len0; i++) {
//     if (uid0[i] != uid1[i]) {
//       return false;
//     }
//   }

//   return true;
// }


/* Local functions */
void DumpMifareClassicToSerial      (MFRC522::Uid *uid, uint8_t piccType, MFRC522::MIFARE_Key *key);
void DumpMifareClassicSectorToSerial(MFRC522::Uid *uid, MFRC522::MIFARE_Key *key, uint8_t sector);
void DumpMifareUltralightToSerial   (void);

static void printHex(uint8_t *buffer, uint8_t bufferSize) {
  for (uint8_t i = 0; i < bufferSize; i++) {
    pc.printf("%02X%s", buffer[i], (i == (bufferSize-1)) ? "" : ":");
  }
}

// static void printDec(uint8_t *buffer, uint8_t bufferSize) {
//   for (uint8_t i = 0; i < bufferSize; i++) {
//     pc.printf("%d%s", buffer[i], (i == (bufferSize-1)) ? "" : ":");
//   }
// }

/**
 * Dumps debug info about the selected PICC to Serial.
 * On success the PICC is halted after dumping the data.
 * For MIFARE Classic the factory default key of 0xFFFFFFFFFFFF is tried.
 */
void DumpToSerial(MFRC522::Uid *uid)
{
  MFRC522::MIFARE_Key key;

  // UID
  pc.printf("Card UID: ");
  printHex(uid->uidByte, uid->size);
  pc.printf("\n");

  // PICC type
  uint8_t piccType = rfid.PICC_GetType(uid->sak);
  pc.printf("PICC Type: %s\n", rfid.PICC_GetTypeName(piccType));


  // Dump contents
  switch (piccType)
  {
    case MFRC522::PICC_TYPE_MIFARE_MINI:
    case MFRC522::PICC_TYPE_MIFARE_1K:
    case MFRC522::PICC_TYPE_MIFARE_4K:
      // All keys are set to FFFFFFFFFFFFh at chip delivery from the factory.
      for (uint8_t i = 0; i < 6; i++) { key.keyByte[i] = 0xFF; }
      DumpMifareClassicToSerial(uid, piccType, &key);
      break;

    case MFRC522::PICC_TYPE_MIFARE_UL:
      DumpMifareUltralightToSerial();
      break;

    case MFRC522::PICC_TYPE_ISO_14443_4:
    case MFRC522::PICC_TYPE_ISO_18092:
    case MFRC522::PICC_TYPE_MIFARE_PLUS:
    case MFRC522::PICC_TYPE_TNP3XXX:
      pc.printf("Dumping memory contents not implemented for that PICC type. \n");
      break;

    case MFRC522::PICC_TYPE_UNKNOWN:
    case MFRC522::PICC_TYPE_NOT_COMPLETE:
    default:
      break; // No memory dump here
  }

  pc.printf("\n");

  rfid.PICC_HaltA(); // Already done if it was a MIFARE Classic PICC.
} // End PICC_DumpToSerial()

/**
 * Dumps memory contents of a MIFARE Classic PICC.
 * On success the PICC is halted after dumping the data.
 */
void DumpMifareClassicToSerial(MFRC522::Uid *uid, uint8_t piccType, MFRC522::MIFARE_Key *key)
{
  uint8_t no_of_sectors = 0;
  switch (piccType)
  {
    case MFRC522::PICC_TYPE_MIFARE_MINI:
      // Has 5 sectors * 4 blocks/sector * 16 bytes/block = 320 bytes.
      no_of_sectors = 5;
      break;

    case MFRC522::PICC_TYPE_MIFARE_1K:
      // Has 16 sectors * 4 blocks/sector * 16 bytes/block = 1024 bytes.
      no_of_sectors = 16;
      break;

    case MFRC522::PICC_TYPE_MIFARE_4K:
      // Has (32 sectors * 4 blocks/sector + 8 sectors * 16 blocks/sector) * 16 bytes/block = 4096 bytes.
      no_of_sectors = 40;
      break;

    default:
      // Should not happen. Ignore.
      break;
  }

  // Dump sectors, highest address first.
  if (no_of_sectors)
  {
    pc.printf("Sector  Block   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  AccessBits\n");
    pc.printf("-----------------------------------------------------------------------------------------\n");
    for (uint8_t i = no_of_sectors - 1; i > 0; i--)
    {
      DumpMifareClassicSectorToSerial(uid, key, i);
    }
  }

  rfid.PICC_HaltA(); // Halt the PICC before stopping the encrypted session.
  rfid.PCD_StopCrypto1();
} // End PICC_DumpMifareClassicToSerial()

/**
 * Dumps memory contents of a sector of a MIFARE Classic PICC.
 * Uses PCD_Authenticate(), MIFARE_Read() and PCD_StopCrypto1.
 * Always uses PICC_CMD_MF_AUTH_KEY_A because only Key A can always read the sector trailer access bits.
 */
void DumpMifareClassicSectorToSerial(MFRC522::Uid *uid, MFRC522::MIFARE_Key *key, uint8_t sector)
{
  uint8_t status;
  uint8_t firstBlock;    // Address of lowest address to dump actually last block dumped)
  uint8_t no_of_blocks;    // Number of blocks in sector
  bool    isSectorTrailer; // Set to true while handling the "last" (ie highest address) in the sector.

  // The access bits are stored in a peculiar fashion.
  // There are four groups:
  //    g[3]  Access bits for the sector trailer, block 3 (for sectors 0-31) or block 15 (for sectors 32-39)
  //    g[2]  Access bits for block 2 (for sectors 0-31) or blocks 10-14 (for sectors 32-39)
  //    g[1]  Access bits for block 1 (for sectors 0-31) or blocks 5-9 (for sectors 32-39)
  //    g[0]  Access bits for block 0 (for sectors 0-31) or blocks 0-4 (for sectors 32-39)
  // Each group has access bits [C1 C2 C3]. In this code C1 is MSB and C3 is LSB.
  // The four CX bits are stored together in a nible cx and an inverted nible cx_.
  uint8_t c1, c2, c3;      // Nibbles
  uint8_t c1_, c2_, c3_;   // Inverted nibbles
  bool    invertedError = false;   // True if one of the inverted nibbles did not match
  uint8_t g[4];            // Access bits for each of the four groups.
  uint8_t group;           // 0-3 - active group for access bits
  bool    firstInGroup;    // True for the first block dumped in the group

  // Determine position and size of sector.
  if (sector < 32)
  { // Sectors 0..31 has 4 blocks each
    no_of_blocks = 4;
    firstBlock = sector * no_of_blocks;
  }
  else if (sector < 40)
  { // Sectors 32-39 has 16 blocks each
    no_of_blocks = 16;
    firstBlock = 128 + (sector - 32) * no_of_blocks;
  }
  else
  { // Illegal input, no MIFARE Classic PICC has more than 40 sectors.
    return;
  }

  // Dump blocks, highest address first.
  uint8_t byteCount;
  uint8_t buffer[18];
  uint8_t blockAddr;
  isSectorTrailer = true;
  for (uint8_t blockOffset = no_of_blocks - 1; blockOffset > 0; blockOffset--)
  {
    blockAddr = firstBlock + blockOffset;

    // Sector number - only on first line
    if (isSectorTrailer)
    {
      pc.printf("  %2d   ", sector);
    }
    else
    {
      pc.printf("       ");
    }

    // Block number
    pc.printf(" %3d  ", blockAddr);

    // Establish encrypted communications before reading the first block
    if (isSectorTrailer)
    {
      status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, firstBlock, key, uid);
      if (status != MFRC522::STATUS_OK)
      {
        pc.printf("PCD_Authenticate() failed: %s\n", rfid.GetStatusCodeName(status));
        return;
      }
    }

    // Read block
    byteCount = sizeof(buffer);
    status = rfid.MIFARE_Read(blockAddr, buffer, &byteCount);
    if (status != MFRC522::STATUS_OK)
    {
      pc.printf("MIFARE_Read() failed: %s\n", rfid.GetStatusCodeName(status));
      continue;
    }

    // Dump data
    for (uint8_t index = 0; index < 16; index++)
    {
      pc.printf(" %3d", buffer[index]);
//      if ((index % 4) == 3)
//      {
//        pc.printf(" ");
//      }
    }

    // Parse sector trailer data
    if (isSectorTrailer)
    {
      c1  = buffer[7] >> 4;
      c2  = buffer[8] & 0xF;
      c3  = buffer[8] >> 4;
      c1_ = buffer[6] & 0xF;
      c2_ = buffer[6] >> 4;
      c3_ = buffer[7] & 0xF;
      invertedError = (c1 != (~c1_ & 0xF)) || (c2 != (~c2_ & 0xF)) || (c3 != (~c3_ & 0xF));

      g[0] = ((c1 & 1) << 2) | ((c2 & 1) << 1) | ((c3 & 1) << 0);
      g[1] = ((c1 & 2) << 1) | ((c2 & 2) << 0) | ((c3 & 2) >> 1);
      g[2] = ((c1 & 4) << 0) | ((c2 & 4) >> 1) | ((c3 & 4) >> 2);
      g[3] = ((c1 & 8) >> 1) | ((c2 & 8) >> 2) | ((c3 & 8) >> 3);
      isSectorTrailer = false;
    }

    // Which access group is this block in?
    if (no_of_blocks == 4)
    {
      group = blockOffset;
      firstInGroup = true;
    }
    else
    {
      group = blockOffset / 5;
      firstInGroup = (group == 3) || (group != (blockOffset + 1) / 5);
    }

    if (firstInGroup)
    {
      // Print access bits
      pc.printf("   [ %d %d %d ] ", (g[group] >> 2) & 1, (g[group] >> 1) & 1, (g[group] >> 0) & 1);
      if (invertedError)
      {
        pc.printf(" Inverted access bits did not match! ");
      }
    }

    if (group != 3 && (g[group] == 1 || g[group] == 6))
    { // Not a sector trailer, a value block
      pc.printf(" Addr = 0x%02X, Value = 0x%02X%02X%02X%02X", buffer[12],
                                                              buffer[3],
                                                              buffer[2],
                                                              buffer[1],
                                                              buffer[0]);
    }

    pc.printf("\n");
  }

  return;
} // End PICC_DumpMifareClassicSectorToSerial()

/**
 * Dumps memory contents of a MIFARE Ultralight PICC.
 */
void DumpMifareUltralightToSerial(void)
{
  uint8_t status;
  uint8_t byteCount;
  uint8_t buffer[18];

  byteCount = sizeof(buffer);
  status = rfid.MIFARE_Read(0, buffer, &byteCount);
  if (status != MFRC522::STATUS_OK)
  {
    pc.printf("MIFARE_Read() of pages 0-3 failed: %s\n", rfid.GetStatusCodeName(status));
    return;
  }

  uint8_t numUserPages = buffer[3*4 + 2] * 2; // Actually * 8 / 4

  pc.printf("Page   0   1   2   3\n");
  // Try the mpages of the original Ultralight. Ultralight C has more pages.
  for (uint8_t pageIdx = 0; pageIdx < numUserPages; pageIdx += 4)
  {
    uint8_t startPage = pageIdx + 4;  // User pages start at page 4

    // Read pages
    byteCount = sizeof(buffer);
    status = rfid.MIFARE_Read(startPage, buffer, &byteCount);
    if (status != MFRC522::STATUS_OK)
    {
      pc.printf("MIFARE_Read() of pages %d-%d failed: %s\n", startPage, startPage + 3, rfid.GetStatusCodeName(status));
      break;
    }

    // Dump data
    for (uint8_t buffPage = 0; buffPage < 4; buffPage++)
    {
      pc.printf(" %3d  ", startPage+buffPage); // Pad with spaces
      for (uint8_t index = 0; index < 4; index++)
      {
        pc.printf(" %02X ", buffer[buffPage*4 + index]);
      }

      pc.printf("\n");
    }
  }
} // End PICC_DumpMifareUltralightToSerial()


/*==============================================================================
 * \brief reset the pn532 chip
 */
// void reset_chip() {
//   rstNFC = 0;
//   wait_ms(100);
//   rstNFC = 1;
// }

/*==============================================================================
 * \brief init the peripheral
 */
void process_esp32_uart() {
  int ch = esp32.getc();

  esp32RXBuffer[esp32RXLen++] = (uint8_t)ch;
  if ((ch == EOF) || (ch == '\n') || (ch == '\0')) {
    if (esp32RXLen > 1) {
      esp32RXBuffer[esp32RXLen-1] = '\0';
      // pc.printf("Recevied: %s\n", esp32RXBuffer);

      // esp32.write(esp32RXBuffer, esp32RXLen, NULL);
      if (strcmp((const char*)esp32RXBuffer, "PLAY_BEEP_01") == 0) {
        commandBuffer.push(CCMD_PLAY_BEEP_01);
      } else if (strcmp((const char*)esp32RXBuffer, "PLAY_BEEP_02") == 0) {
        commandBuffer.push(CCMD_PLAY_BEEP_02);
      } else if (strcmp((const char*)esp32RXBuffer, "PLAY_BEEP_03") == 0) {
        commandBuffer.push(CCMD_PLAY_BEEP_03);
      } else if (strcmp((const char*)esp32RXBuffer, "PLAY_BEEP_04") == 0) {
        commandBuffer.push(CCMD_PLAY_BEEP_04);
      } else if (strcmp((const char*)esp32RXBuffer, "PLAY_BEEP_05") == 0) {
        commandBuffer.push(CCMD_PLAY_BEEP_05);
      } else if (strcmp((const char*)esp32RXBuffer, "PLAY_BEEP_06") == 0) {
        commandBuffer.push(CCMD_PLAY_BEEP_06);
      } else if (strcmp((const char*)esp32RXBuffer, "PLAY_BUZZER_01") == 0) {
        commandBuffer.push(CCMD_PLAY_BUZZER_01);
      } else if (strcmp((const char*)esp32RXBuffer, "PLAY_BUZZER_02") == 0) {
        commandBuffer.push(CCMD_PLAY_BUZZER_02);
      } else if (strcmp((const char*)esp32RXBuffer, "PLAY_SUCCESS") == 0) {
        commandBuffer.push(CCMD_PLAY_SUCCESS);
      } else if (strcmp((const char*)esp32RXBuffer, "PLAY_FAILURE") == 0) {
        commandBuffer.push(CCMD_PLAY_FAILURE);
      } else if (strcmp((const char*)esp32RXBuffer, "PLAY_SMB") == 0) {
        commandBuffer.push(CCMD_PLAY_SMB);
      } else if (strcmp((const char*)esp32RXBuffer, "LOCK_DOOR") == 0) {
        commandBuffer.push(CCMD_LOCK_DOOR);
      } else if (strcmp((const char*)esp32RXBuffer, "UNLOCK_DOOR") == 0) {
        commandBuffer.push(CCMD_UNLOCK_DOOR);
      }
    }

    esp32RXLen = 0;
  } else {
    if (esp32RXLen >= ((int)ARRAY_COUNT(esp32RXBuffer) - 1)) {
      // Erk... just wrap if the buffer's full without a newline
      esp32RXLen = 0;

      pc.printf("ERROR: Read buffer overflow from ESP32!\n");
    }
  }
}

/*==============================================================================
 * \brief key was pressed on the keypad
 */
static uint32_t onKeypadPressed(uint32_t index) {
  char keyCode = Keytable[index];

  if (keyCode == '*') {
    pinCompleted = true;
  } else {
    int pinLen = strlen(pinCode);
    pinCode[pinLen++] = keyCode;
    pinCode[pinLen] = '\0';

    if (pinLen >= 8) {
      pinCompleted = true;
    }
  }

  commandBuffer.push(CCMD_PLAY_BEEP_01);

  pinTimeout.reset();

  return 0;
}

/*==============================================================================
 * \brief init the peripheral
 */
void setup() {
  pc.printf("MFRC522 initializing...\n");

  esp32.attach(&process_esp32_uart);

  // buz.period(1.0f / 600.0f);

  rfid.PCD_Init();

  /* Read RC522 version */
  uint8_t rc522_version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  pc.printf("MFRC522 version: %d\n", rc522_version & 0x07);

  //
  pinCode[0] = '\0';
  pinCompleted = false;
  pinTimeout.start();
  keypad.attach(&onKeypadPressed);
  keypad.start();

  // rfid.PCD_Init();
  // reset_chip();

  // for (uint8_t i = 0; i < 6; i++) {
  //   key.keyByte[i] = 0xFF;
  // }

  // for (byte i = 0; i < 4; i++) {
  //   nuidPICC[i] =0xFF;
  // }
  
  // uint32_t versiondata = 0;
  // while (1) {
  //   nfc.begin();
  //   //nfc.SAMConfig();
  //   versiondata = nfc.getFirmwareVersion();
  //   if (!versiondata)
  //   {
  //     pc.printf("Didn't find PN53x board\n\n");
  //     wait_ms(500);
  //   }
  //   else
  //   {
  //     break;
  //   }
  // }

  // Got ok data, print it out!
  // pc.printf("Found chip PN5%02X , Firmware ver. %d.%d\n",
  //           (versiondata >> 24) & 0xFF,
  //           (versiondata >> 16) & 0xFF,
  //           (versiondata >> 8) & 0xFF);

  // Set the max number of retry attempts to read from a card
  // This prevents us from waiting forever for a card, which is
  // the default behaviour of the PN532.
  // nfc.setPassiveActivationRetries(0xFF);

  // configure board to read RFID tags
  // nfc.SAMConfig();

  pc.printf("\nWaiting for an ISO14443A card.\n");

  esp32.printf("STM32_READY\n");
}

// struct note_t {
//   float note;
//   float t;
// };
// const float bpm = 120.0f;
// const float qnote = 0.25f / (bpm / 60.0f);
// const note_t song1[] = {
//   {C6, qnote},
//   {C6, qnote},
//   {E6, qnote},
//   {E6, qnote},
//   {G6, qnote},
//   {G6, qnote},
//   {E6, qnote*2.0f},
//   {D6, qnote},
//   {D6, qnote},
//   {F6, qnote},
//   {F6, qnote},
//   {E6, qnote},
//   {E6, qnote},
//   // {B5, qnote},
// };
// const note_t song2[] = {
//   {C3, qnote},
//   {B2, qnote},
//   {C4, qnote},
//   {B3, qnote},
//   {C5, qnote},
//   {D5, qnote},
// };

// void playSong(const note_t* pSong, uint8_t numNotes) {
//   for (uint8_t n=0; n<numNotes; n++) {
//     buzzer.delayBeep(pSong[n].note, pSong[n].t);
//   }
// }

const char* BEEP_01 = "T200 L6 O3 C";
const char* BEEP_02 = "T200 L6 O2 C";
const char* BEEP_03 = "T200 L1 O3 C";
const char* BEEP_04 = "T200 L1 O2 C";
const char* BEEP_05 = "T200 L6 O4 C";
const char* BEEP_06 = "T200 L1 O4 C";
const char* BUZZER_01 = "T200 L16 O4 CDEF CDEF CDEF CDEF";
const char* BUZZER_02 = "T200 L16 O3 CD O4 EF O3 CD O4 EF";

// const char* DOODLY = "O3>CG8G8AGPB>C";
const char* YAY = "O4L32MLCDEFG";		//success
const char* WAH = "T80O2L32GFEDC";		//failure
// Super Mario Brothers theme (11 notes)
const char* smb = {
	"T180 O3 E8 E8 P8 E8 P8 C8 D#4 G4 P4 <G4 P4"
};
// The Entertainer by Scott Joplin
// const char* entertainer = {
// 	":The Entertainer by Scott Joplin\n"	//comment lines need a \n
// 	""
// 	"T80            :Prelude\n"
// 	""
// 	"O4MLL64C#MNL16DECO3L8AL16BMSL8G"
// 	"MLL64C#MNL16DECO2L8AL16BL8MSGMN"
// 	"O2MLL64C#MNL16DECO1L8AL16BAA-"
// 	"MSL8GMNP4O3L8GO2L16MLDMND#"
// 	""
// 	"MLO2L16EMSL8O3CMLO2L16EO3MSL8C"
// 	"MNO2L16EO3L4C.L16O4CDD#"
// 	"ECDL8EL16O3BO4MSL8D"
// 	"MNCL16O3MLEGO4MSL8CMLO2L16DMND#"
// 	"MLO2L16EMSL8O3CMLO2L16EO3MSL8C"
// 	"MLO2L16EO3L4C.L16MLAMNG"
// 	"F#AO4CL8EL16DCO3A"
// 	"O4L4D.O1L16MLDMND#"
// 	"MLO2L16EMSL8O3CMLO2L16EO3MSL8C"
// 	"MNO2L16EO3L4C.L16O4CDD#"
// 	"ECDL8EL16O3BO4MSL8D"
// 	"MNCL16O3MLEGO4MSL8CMNL16CD"
// 	"ECDL8EL16CDC"
// 	"ECDL8EL16CDC"
// 	"ECDL8EL16O3BO4L8MSDMN"
// 	"L5MLC.MNL16O3EFF#"
// 	""
// 	"O3L8MSGMNL16AL8GL16EFF#"
// 	"MSL8GMNL16AL8GL16MSECO2G"
// 	"MLL16ABO3CDEDCD"
// 	"MNO2GMLO3EFGAGEMNF"
// 	"L8MSGMNL16AL8GL16EFF#"
// 	"L8MSGMNL16AL8GL16GAA#"
// 	"BL8BL16L8BL16AF#D"
// 	"L5MLG.MNL16EFF#"
// 	"MSL8GMNL16AL8GL16EFF#"
// 	"L8MSGMNL16AL8GL16MLECO3MNG"
// 	"MLABO4CDEDCD"
// 	"MSL8MLCL16EGMSO5CO4MNGF#G"
// 	"MSO5L8CO4MNL16AO5L8CL16O4AO5CO4A"
// 	"GO5CEL8GL16ECO4G"
// 	"MSL8AO5CMNL16EL8D"
// 	"L4C..L16O4EFF#"
// 	"MSL8GMNL16AL8GL16EFF#"
// 	"L8GL16AL8GL16MSECO3G"
// 	"MNABO4CDEDCD"
// 	"O3GO4EFGAGEF"
// 	"MSL8GMNL16AL8GL16EFF#"
// 	"L8GL16AL8GL16GAA#"
// 	""
// 	"BL8BBL16AF#D"
// 	"MLL64GAGAGAGAGAGAGAGAGL16MNEFF#"
// 	"L8MSGL16AL8GL16EFF#"
// 	"L8GL16AL8GL16MSECO3G"
// 	"MNABO4CDEDCD"
// 	"MSL8CL16MLEGO5MNCO4GF#G"
// 	"O5MSL8CO4MNL16AMLO5L8CL16O4AO5"
// 	"CO4AGO5CEL8GL16ECO4G"
// 	"L8MSAO5CMNL16EL8DL16MLC"
// 	"L4CL8C"
// };

// // The M.GAKKOU KOUKA
// // copyright "Music Composed by Kenkichi Motoi 2009 Wikimedia version 2012"
// const char* gakkou = {	//play with sticky shift?
// 	"T160 O3L4"
// 	"ED8CE8 GG8ER8 AA8>C<A8 G2R"
// 	"AA8GA8 >CC8D<R8 EE8DE8 C2R"
// 	"DD8DD8 DD8DR8 ED8EF8 G2R"
// 	"AA8GA8 >CC8<AR8 >DC8DE8 D2<R"
// 	">EE8DC8< AB8>CC8< GG8EA8 G2R"
// 	">CC8<GE8 CD8EA8 GG8DE8 C2R"
// };

/*==============================================================================
 * \brief find a tag
 */
// int tone = 100;
void loop() {
  // PIN
  if (pinTimeout.read_ms() > 5000) {
    // Send whatever has been entered already
    // Minimum 5 characters (single digit user ID, 4 digit pin)
    if (strlen(pinCode) >= 5) {
      pinCompleted = true;
    }
  }

  if (pinCompleted) {
    esp32.printf("PIN:%s\n", pinCode);
    pc.printf("PIN completed: %s\n", pinCode);

    pinCode[0] = '\0';
    pinCompleted = false;
  }

  // Process pending commands
  ControlCmd cmd = CCMD_NOP;
  while (commandBuffer.pop(cmd)) {
    if (cmd == CCMD_PLAY_BEEP_01) {
      audioPlayback.play(BEEP_01);
    } else if (cmd == CCMD_PLAY_BEEP_02) {
      audioPlayback.play(BEEP_02);
    } else if (cmd == CCMD_PLAY_BEEP_03) {
      audioPlayback.play(BEEP_03);
    } else if (cmd == CCMD_PLAY_BEEP_04) {
      audioPlayback.play(BEEP_04);
    } else if (cmd == CCMD_PLAY_BEEP_05) {
      audioPlayback.play(BEEP_05);
    } else if (cmd == CCMD_PLAY_BEEP_06) {
      audioPlayback.play(BEEP_06);
    } else if (cmd == CCMD_PLAY_BUZZER_01) {
      audioPlayback.play(BUZZER_01);
    } else if (cmd == CCMD_PLAY_BUZZER_02) {
      audioPlayback.play(BUZZER_02);
    } else if (cmd == CCMD_PLAY_SUCCESS) {
      audioPlayback.play(YAY);
    } else if (cmd == CCMD_PLAY_FAILURE) {
      audioPlayback.play(WAH);
    } else if (cmd == CCMD_PLAY_SMB) {
      audioPlayback.play(smb);
    } else if (cmd == CCMD_LOCK_DOOR) {
      ledNFC = 1; // led off
      doorRelay = 0;
    } else if (cmd == CCMD_UNLOCK_DOOR) {
      ledNFC = 0; // led on
      doorRelay = 1;
    }
  }

  // Look for new cards
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  // Verify if the NUID has been readed
  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  // buz.period(1.0f / (float)tone);
  // pc.printf("Buzzer tone: %dHz\n\n", tone);
  // tone += 100;
  // buz.write(0.5f);
  ledNFC = 0;     // led on

  DumpToSerial(&rfid.uid);

  esp32.printf("RFID:");
  for (int i=0; i<rfid.uid.size; i++) {
    esp32.putc(rfid.uid.uidByte[i]);
  }
  esp32.putc('\n');

  // buzzer.sing(Buzzer::POST_SOUND);
  // buzzer.delayBeep(C5, btime);
  // buzzer.delayBeep(C6, btime);
  // buzzer.delayBeep(D6, btime);
  // buzzer.delayBeep(E6, btime);
  // playSong(song1, ARRAY_COUNT(song1));
  // audioPlayback.play(YAY);
  // wait_ms(400);
  // buz.write(0.0f);
  ledNFC = 1; // led off
 
  // if (compareCards(rfid.uid.uidByte, rfid.uid.size, valid_card, ARRAY_COUNT(valid_card))) {
  //   audioPlayback.play(smb); 
  // } else {
  //   audioPlayback.play(WAH);
  // }

  // MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  // pc.printf("PICC type: %s\n", rfid.PICC_GetTypeName(piccType));

  // // Check is the PICC of Classic MIFARE type
  // if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
  //   piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
  //   piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
  //   pc.printf("Your tag is not of type MIFARE Classic.\n");
  //   return;
  // }

  // if (rfid.uid.uidByte[0] != nuidPICC[0] || 
  //   rfid.uid.uidByte[1] != nuidPICC[1] || 
  //   rfid.uid.uidByte[2] != nuidPICC[2] || 
  //   rfid.uid.uidByte[3] != nuidPICC[3] ) {
  //   tone(buz, 800); // turn on the buzzer
  //   ledNFC = 0;     // led on

  //   pc.printf("A new card has been detected.\n");

  //   // Store NUID into nuidPICC array
  //   for (uint8_t i = 0; i < 4; i++) {
  //     nuidPICC[i] = rfid.uid.uidByte[i];
  //   }
   
  //   pc.printf("The NUID tag is:\n");
  //   pc.printf("In hex: ");
  //   printHex(rfid.uid.uidByte, rfid.uid.size);
  //   pc.printf("\nIn decimal: ");
  //   printDec(rfid.uid.uidByte, rfid.uid.size);
  //   pc.printf("\n\n");

  //   wait_ms(100);
  //   tone(buz, 0); // turn off the buzzer
  //   ledNFC = 1; // led off
  // }
  // else {
  //   pc.printf("Card read previously.\n");
  // }

  // // Halt PICC
  // rfid.PICC_HaltA();

  // // Stop encryption on PCD
  // rfid.PCD_StopCrypto1();


  // bool success;
  // uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer to store the returned UID
  // uint8_t uidLength;                     // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  // // configure board to read RFID tags
  // nfc.SAMConfig();

  // // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // // 'uid' will be populated with the UID, and uidLength will indicate
  // // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  // success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);

  // pc.printf("\n");

  // if (success)
  // {
  //   tone(buz, 800); // turn on the buzzer
  //   ledNFC = 0;     // led on

  //   pc.printf("Found a card!\n");

  //   pc.printf("UID Length: %d bytes\n", uidLength);
  //   pc.printf("UID Value: ");

  //   for (uint8_t i = 0; i < uidLength; i++)
  //     pc.printf(" 0x%02X", uid[i]);

  //   pc.printf("\n");

  //   wait_ms(100);
  //   tone(buz, 0); // turn off the buzzer

  //   // wait until the card is taken away
  //   while (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength, 500))
  //   {
  //   }

  //   ledNFC = 1; // led off
  // }
  // else
  // {
  //   // PN532 probably timed out waiting for a card
  //   pc.printf("\nTimed out waiting for a card\n");
  //   ledNFC = 1;
  //   wait_ms(200);
  // }
}

/*==============================================================================
 * \brief main entry
 */
int main() {
    ledNFC = 1; // led off
    doorRelay = 0;

  // audioPlayback.play(WAH);
  // wait_ms(2000);
  // wait_ms(2000);
  // audioPlayback.play(YAY);

  // for (int i=0; i<3; i++) {
  //   ledNFC = 0; // led on
  //   doorRelay = 1;
  //   wait_ms(500);

  //   ledNFC = 1; // led off
  //   doorRelay = 0;
  //   wait_ms(500);
  // }

  setup();

  while (1) {
    loop();
    // wait_ms(200);
  }
}
