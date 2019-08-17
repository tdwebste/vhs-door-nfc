#include <mbed.h>
#include <Timer.h>
#include <platform/CircularBuffer.h>

#include <MFRC522.h>
#include <PwmSound.h>
#include <Keypad.h>

#include "nfc_debug.h"

#define ARRAY_COUNT(arr) (sizeof(arr) / (sizeof((arr)[0])))

static DigitalOut ledNFC(PC_13);

static DigitalOut doorRelay(PA_0);

static PwmSound audioPlayback(PB_1);

static const char Keytable[] = {
    '1', '2', '3', // r0
    '4', '5', '6', // r1
    '7', '8', '9', // r2
    '*', '0', '#', // r3
};
//  c0   c1   c2

static Keypad keypad(PB_0, PA_7, PA_6, PA_5, // rows
                     PA_4, PA_3, PA_2, 200); // columns

static char  pinCode[16]  = {};
static bool  pinCompleted = false;
static Timer pinTimeout;

// Serial pc(PA_9, PA_10, 115200);

// static RawSerial esp32(PB_10, PB_11, 115200);
// static uint8_t   esp32RXBuffer[64] = {};
// static int       esp32RXLen        = 0;

static CAN can(PB_8, PB_9, 1000000);
const uint32_t CAN_ID_STM32     = 0x7AA;    // 1962
const uint32_t CAN_ID_ESP32     = 0x755;    // 1877

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
    CCMD_UNLOCK_DOOR,

    CCMD_Count
};

static CircularBuffer<ControlCmd, 16, uint8_t> commandBuffer;

#define SPI2_MOSI PB_15
#define SPI2_MISO PB_14
#define SPI2_SCK PB_13
#define MFRC522_SS_PIN PB_12
#define MFRC522_RST_PIN PA_8

MFRC522 rfid(SPI2_MOSI, SPI2_MISO, SPI2_SCK, MFRC522_SS_PIN, MFRC522_RST_PIN);


// static void reset_chip() {
//   rstNFC = 0;
//   wait_ms(100);
//   rstNFC = 1;
// }

union uchar2int_ptr {
    unsigned char* uchar_ptr;
    int* int_ptr;
};

static void process_can_message() {
    CANMessage msg;
    if (can.read(msg)) {
        if (msg.len == sizeof(ControlCmd)) {
            // Work-around strict-aliasing rules by casting through a union
            uchar2int_ptr caster;
            caster.uchar_ptr = msg.data;
            int icmd = *caster.int_ptr;

            if ((icmd >= 0) && (icmd < CCMD_Count)) {
                ControlCmd cmd = (ControlCmd)icmd;

                commandBuffer.push(cmd);
            }
        }
    }
}

// static void process_esp32_uart() {
//     int ch = esp32.getc();

//     esp32RXBuffer[esp32RXLen++] = (uint8_t)ch;
//     if ((ch == EOF) || (ch == '\n') || (ch == '\0')) {
//         if (esp32RXLen > 1) {
//             esp32RXBuffer[esp32RXLen - 1] = '\0';

//             if (strcmp((const char*)esp32RXBuffer, "PLAY_BEEP_01") == 0) {
//                 commandBuffer.push(CCMD_PLAY_BEEP_01);
//             } else if (strcmp((const char*)esp32RXBuffer, "PLAY_BEEP_02") == 0) {
//                 commandBuffer.push(CCMD_PLAY_BEEP_02);
//             } else if (strcmp((const char*)esp32RXBuffer, "PLAY_BEEP_03") == 0) {
//                 commandBuffer.push(CCMD_PLAY_BEEP_03);
//             } else if (strcmp((const char*)esp32RXBuffer, "PLAY_BEEP_04") == 0) {
//                 commandBuffer.push(CCMD_PLAY_BEEP_04);
//             } else if (strcmp((const char*)esp32RXBuffer, "PLAY_BEEP_05") == 0) {
//                 commandBuffer.push(CCMD_PLAY_BEEP_05);
//             } else if (strcmp((const char*)esp32RXBuffer, "PLAY_BEEP_06") == 0) {
//                 commandBuffer.push(CCMD_PLAY_BEEP_06);
//             } else if (strcmp((const char*)esp32RXBuffer, "PLAY_BUZZER_01") == 0) {
//                 commandBuffer.push(CCMD_PLAY_BUZZER_01);
//             } else if (strcmp((const char*)esp32RXBuffer, "PLAY_BUZZER_02") == 0) {
//                 commandBuffer.push(CCMD_PLAY_BUZZER_02);
//             } else if (strcmp((const char*)esp32RXBuffer, "PLAY_SUCCESS") == 0) {
//                 commandBuffer.push(CCMD_PLAY_SUCCESS);
//             } else if (strcmp((const char*)esp32RXBuffer, "PLAY_FAILURE") == 0) {
//                 commandBuffer.push(CCMD_PLAY_FAILURE);
//             } else if (strcmp((const char*)esp32RXBuffer, "PLAY_SMB") == 0) {
//                 commandBuffer.push(CCMD_PLAY_SMB);
//             } else if (strcmp((const char*)esp32RXBuffer, "LOCK_DOOR") == 0) {
//                 commandBuffer.push(CCMD_LOCK_DOOR);
//             } else if (strcmp((const char*)esp32RXBuffer, "UNLOCK_DOOR") == 0) {
//                 commandBuffer.push(CCMD_UNLOCK_DOOR);
//             }
//         }

//         esp32RXLen = 0;
//     } else {
//         if (esp32RXLen >= ((int)ARRAY_COUNT(esp32RXBuffer) - 1)) {
//             // Erk... just wrap if the buffer's full without a newline
//             esp32RXLen = 0;

            // pc.printf("ERROR: Read buffer overflow from ESP32!\n");
//         }
//     }
// }

static uint32_t onKeypadPressed(uint32_t index) {
    char keyCode = Keytable[index];

    if (keyCode == '*') {
        pinCompleted = true;
    } else {
        int pinLen        = strlen(pinCode);
        pinCode[pinLen++] = keyCode;
        pinCode[pinLen]   = '\0';

        if (pinLen >= 8) {
            pinCompleted = true;
        }
    }

    commandBuffer.push(CCMD_PLAY_BEEP_01);

    pinTimeout.reset();

    return 0;
}

static void setup() {
    // pc.printf("MFRC522 initializing...\n");

    can.filter(CAN_ID_ESP32, ~0u, CANStandard);
    can.attach(process_can_message);

    // esp32.attach(&process_esp32_uart);

    rfid.PCD_Init();

    /* Read RC522 version */
    // uint8_t rc522_version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
    // pc.printf("MFRC522 version: %d\n", rc522_version & 0x07);

    //
    pinCode[0]   = '\0';
    pinCompleted = false;
    pinTimeout.start();
    keypad.attach(&onKeypadPressed);
    keypad.start();

    // reset_chip();

    // pc.printf("\nWaiting for an ISO14443A card.\n");

    // esp32.printf("STM32_READY\n");
}

const char* BEEP_01   = "T200 L6 O3 C";
const char* BEEP_02   = "T200 L6 O2 C";
const char* BEEP_03   = "T200 L1 O3 C";
const char* BEEP_04   = "T200 L1 O2 C";
const char* BEEP_05   = "T200 L6 O4 C";
const char* BEEP_06   = "T200 L1 O4 C";
const char* BUZZER_01 = "T200 L16 O4 CDEF CDEF CDEF CDEF";
const char* BUZZER_02 = "T200 L16 O3 CD O4 EF O3 CD O4 EF";

// const char* DOODLY = "O3>CG8G8AGPB>C";
const char* YAY = "O4L32MLCDEFG";  //success
const char* WAH = "T80O2L32GFEDC"; //failure
// Super Mario Brothers theme (11 notes)
const char* smb = {
    "T180 O3 E8 E8 P8 E8 P8 C8 D#4 G4 P4 <G4 P4"
};

#define RFID_MSG_PREFIX "RFID:"
#define PIN_MSG_PREFIX "PIN:"
#define RFID_MSG_PREFIX_LEN     (ARRAY_COUNT(RFID_MSG_PREFIX) - 1)
#define PIN_MSG_PREFIX_LEN      (ARRAY_COUNT(PIN_MSG_PREFIX) - 1)

static void send_can(unsigned char* pBuffer, uint32_t size) {
    uint8_t packets = (size + 5) / 6;

    CANMessage msg;
    for (uint8_t i=0; i<packets; i++) {
        memset(&msg, 0, sizeof(msg));

        msg.id = CAN_ID_STM32;
        msg.format = CANStandard;
        msg.type = CANData;
        msg.len = (i != (packets - 1)) ? 8 : 2 + (size % 6);

        msg.data[0] = i + 1;
        msg.data[1] = packets;
        memcpy(msg.data+2, pBuffer, msg.len-2);
        pBuffer += msg.len-2;
    }
}

static void process_rfid() {
    // Look for new cards
    if (!rfid.PICC_IsNewCardPresent()) {
        return;
    }

    // Verify if the NUID has been readed
    if (!rfid.PICC_ReadCardSerial()) {
        return;
    }

    ledNFC = 0; // led on

    DumpToSerial(&rfid.uid);

    unsigned char rfid_msg[RFID_MSG_PREFIX_LEN + ARRAY_COUNT(rfid.uid.uidByte)];
    memset(rfid_msg, 0, sizeof(rfid_msg));
    strcpy((char*)rfid_msg, RFID_MSG_PREFIX);
    // esp32.printf(RFID_MSG_PREFIX);
    for (int i = 0; i < rfid.uid.size; i++) {
        rfid_msg[RFID_MSG_PREFIX_LEN + i] = rfid.uid.uidByte[i];
        // esp32.putc(rfid.uid.uidByte[i]);
    }
    send_can(rfid_msg, RFID_MSG_PREFIX_LEN + rfid.uid.size);
    // esp32.putc('\n');

    ledNFC = 1; // led off
}

static void loop() {
    // PIN
    if (pinTimeout.read_ms() > 5000) {
        // Send whatever has been entered already
        pinCompleted = true;
    }

    if (pinCompleted) {
        // Minimum 5 characters (single digit user ID, 4 digit pin)
        if (strlen(pinCode) >= 5) {
            unsigned char pin_msg[PIN_MSG_PREFIX_LEN + 8];
            memset(pin_msg, 0, sizeof(pin_msg));
            strcpy((char*)pin_msg, PIN_MSG_PREFIX);
            for (unsigned int i=0; i<strlen(pinCode); i++) {
                pin_msg[PIN_MSG_PREFIX_LEN + i] = pinCode[i];
            }
            send_can(pin_msg, PIN_MSG_PREFIX_LEN + strlen(pinCode));
            // esp32.printf(PIN_MSG_PREFIX);
            // esp32.printf("%s\n", pinCode);
            // pc.printf("PIN completed: %s\n", pinCode);
        }

        pinCode[0]   = '\0';
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
            ledNFC    = 1; // led off
            doorRelay = 0;
        } else if (cmd == CCMD_UNLOCK_DOOR) {
            ledNFC    = 0; // led on
            doorRelay = 1;
        }
    }

    // Check for and process if an RFID card is present
    process_rfid();
    // DumpToSerial(&rfid.uid);
}

int main() {
    ledNFC    = 1; // led off
    doorRelay = 0;

    setup();

    while (1) {
        loop();
    }
}
