#include <mbed.h>

#include <MFRC522.h>

#include "nfc_debug.h"


extern Serial  pc;
extern MFRC522 rfid;


static void printHex(uint8_t* buffer, uint8_t bufferSize) {
    for (uint8_t i = 0; i < bufferSize; i++) {
        pc.printf("%02X%s", buffer[i], (i == (bufferSize - 1)) ? "" : ":");
    }
}

// static void printDec(uint8_t *buffer, uint8_t bufferSize) {
//   for (uint8_t i = 0; i < bufferSize; i++) {
//     pc.printf("%d%s", buffer[i], (i == (bufferSize-1)) ? "" : ":");
//   }
// }

static void DumpMifareClassicToSerial(MFRC522::Uid* uid, uint8_t piccType, MFRC522::MIFARE_Key* key);
static void DumpMifareClassicSectorToSerial(MFRC522::Uid* uid, MFRC522::MIFARE_Key* key, uint8_t sector);
static void DumpMifareUltralightToSerial(void);

/**
 * Dumps debug info about the selected PICC to Serial.
 * On success the PICC is halted after dumping the data.
 * For MIFARE Classic the factory default key of 0xFFFFFFFFFFFF is tried.
 */
void DumpToSerial(MFRC522::Uid* uid) {
    MFRC522::MIFARE_Key key;

    // UID
    pc.printf("Card UID: ");
    printHex(uid->uidByte, uid->size);
    pc.printf("\n");

    // PICC type
    uint8_t piccType = rfid.PICC_GetType(uid->sak);
    pc.printf("PICC Type: %s\n", rfid.PICC_GetTypeName(piccType));

    // Dump contents
    switch (piccType) {
    case MFRC522::PICC_TYPE_MIFARE_MINI:
    case MFRC522::PICC_TYPE_MIFARE_1K:
    case MFRC522::PICC_TYPE_MIFARE_4K:
        // All keys are set to FFFFFFFFFFFFh at chip delivery from the factory.
        for (uint8_t i = 0; i < 6; i++) {
            key.keyByte[i] = 0xFF;
        }
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
static void DumpMifareClassicToSerial(MFRC522::Uid* uid, uint8_t piccType, MFRC522::MIFARE_Key* key) {
    uint8_t no_of_sectors = 0;
    switch (piccType) {
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
    if (no_of_sectors) {
        pc.printf("Sector  Block   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  AccessBits\n");
        pc.printf("-----------------------------------------------------------------------------------------\n");
        for (uint8_t i = no_of_sectors - 1; i > 0; i--) {
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
static void DumpMifareClassicSectorToSerial(MFRC522::Uid* uid, MFRC522::MIFARE_Key* key, uint8_t sector) {
    uint8_t status;
    uint8_t firstBlock;      // Address of lowest address to dump actually last block dumped)
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
    uint8_t c1, c2, c3;            // Nibbles
    uint8_t c1_, c2_, c3_;         // Inverted nibbles
    bool    invertedError = false; // True if one of the inverted nibbles did not match
    uint8_t g[4];                  // Access bits for each of the four groups.
    uint8_t group;                 // 0-3 - active group for access bits
    bool    firstInGroup;          // True for the first block dumped in the group

    // Determine position and size of sector.
    if (sector < 32) { // Sectors 0..31 has 4 blocks each
        no_of_blocks = 4;
        firstBlock   = sector * no_of_blocks;
    } else if (sector < 40) { // Sectors 32-39 has 16 blocks each
        no_of_blocks = 16;
        firstBlock   = 128 + (sector - 32) * no_of_blocks;
    } else { // Illegal input, no MIFARE Classic PICC has more than 40 sectors.
        return;
    }

    // Dump blocks, highest address first.
    uint8_t byteCount;
    uint8_t buffer[18];
    uint8_t blockAddr;
    isSectorTrailer = true;
    for (uint8_t blockOffset = no_of_blocks - 1; blockOffset > 0; blockOffset--) {
        blockAddr = firstBlock + blockOffset;

        // Sector number - only on first line
        if (isSectorTrailer) {
            pc.printf("  %2d   ", sector);
        } else {
            pc.printf("       ");
        }

        // Block number
        pc.printf(" %3d  ", blockAddr);

        // Establish encrypted communications before reading the first block
        if (isSectorTrailer) {
            status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, firstBlock, key, uid);
            if (status != MFRC522::STATUS_OK) {
                pc.printf("PCD_Authenticate() failed: %s\n", rfid.GetStatusCodeName(status));
                return;
            }
        }

        // Read block
        byteCount = sizeof(buffer);
        status    = rfid.MIFARE_Read(blockAddr, buffer, &byteCount);
        if (status != MFRC522::STATUS_OK) {
            pc.printf("MIFARE_Read() failed: %s\n", rfid.GetStatusCodeName(status));
            continue;
        }

        // Dump data
        for (uint8_t index = 0; index < 16; index++) {
            pc.printf(" %3d", buffer[index]);
            //      if ((index % 4) == 3)
            //      {
            //        pc.printf(" ");
            //      }
        }

        // Parse sector trailer data
        if (isSectorTrailer) {
            c1            = buffer[7] >> 4;
            c2            = buffer[8] & 0xF;
            c3            = buffer[8] >> 4;
            c1_           = buffer[6] & 0xF;
            c2_           = buffer[6] >> 4;
            c3_           = buffer[7] & 0xF;
            invertedError = (c1 != (~c1_ & 0xF)) || (c2 != (~c2_ & 0xF)) || (c3 != (~c3_ & 0xF));

            g[0]            = ((c1 & 1) << 2) | ((c2 & 1) << 1) | ((c3 & 1) << 0);
            g[1]            = ((c1 & 2) << 1) | ((c2 & 2) << 0) | ((c3 & 2) >> 1);
            g[2]            = ((c1 & 4) << 0) | ((c2 & 4) >> 1) | ((c3 & 4) >> 2);
            g[3]            = ((c1 & 8) >> 1) | ((c2 & 8) >> 2) | ((c3 & 8) >> 3);
            isSectorTrailer = false;
        }

        // Which access group is this block in?
        if (no_of_blocks == 4) {
            group        = blockOffset;
            firstInGroup = true;
        } else {
            group        = blockOffset / 5;
            firstInGroup = (group == 3) || (group != (blockOffset + 1) / 5);
        }

        if (firstInGroup) {
            // Print access bits
            pc.printf("   [ %d %d %d ] ", (g[group] >> 2) & 1, (g[group] >> 1) & 1, (g[group] >> 0) & 1);
            if (invertedError) {
                pc.printf(" Inverted access bits did not match! ");
            }
        }

        if (group != 3 && (g[group] == 1 || g[group] == 6)) { // Not a sector trailer, a value block
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
static void DumpMifareUltralightToSerial(void) {
    uint8_t status;
    uint8_t byteCount;
    uint8_t buffer[18];

    byteCount = sizeof(buffer);
    status    = rfid.MIFARE_Read(0, buffer, &byteCount);
    if (status != MFRC522::STATUS_OK) {
        pc.printf("MIFARE_Read() of pages 0-3 failed: %s\n", rfid.GetStatusCodeName(status));
        return;
    }

    uint8_t numUserPages = buffer[3 * 4 + 2] * 2; // Actually * 8 / 4

    pc.printf("Page   0   1   2   3\n");
    // Try the mpages of the original Ultralight. Ultralight C has more pages.
    for (uint8_t pageIdx = 0; pageIdx < numUserPages; pageIdx += 4) {
        uint8_t startPage = pageIdx + 4; // User pages start at page 4

        // Read pages
        byteCount = sizeof(buffer);
        status    = rfid.MIFARE_Read(startPage, buffer, &byteCount);
        if (status != MFRC522::STATUS_OK) {
            pc.printf("MIFARE_Read() of pages %d-%d failed: %s\n", startPage, startPage + 3, rfid.GetStatusCodeName(status));
            break;
        }

        // Dump data
        for (uint8_t buffPage = 0; buffPage < 4; buffPage++) {
            pc.printf(" %3d  ", startPage + buffPage); // Pad with spaces
            for (uint8_t index = 0; index < 4; index++) {
                pc.printf(" %02X ", buffer[buffPage * 4 + index]);
            }

            pc.printf("\n");
        }
    }
} // End PICC_DumpMifareUltralightToSerial()
