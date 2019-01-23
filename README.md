# vhs-door-nfc
Space access using NFC + PIN code

## Building Firmware
You will need an API key that has the 'PIN Authentication Service' and 'RFID Authentication Service' privileges.

1. You will need Visual Studio Code with the PlatformIO extension installed as the firmware projects are set up with Visual Studio Code workspaces using PlatformIO for the build system.
1. Obtain the API key from Nomos and grant it the required permissions.
1. Create the file software/esp32-firmware/src/nomos_api_key.h
1. Edit the file to contain
```C
#ifndef __NOMOS_API_KEY_H__
#define __NOMOS_API_KEY_H__

#ifndef NOMOS_API_KEY
#define NOMOS_API_KEY "put_your_api_key_here"
#endif //NOMOS_API_KEY

#endif //__NOMOS_API_KEY_H__
```
1. In VSCode you should be able to use the regular PlatformIO build and upload commands for each workspace.
