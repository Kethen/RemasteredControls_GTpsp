### Remastered Controls for GTpsp

- override internal steering deadzone for fine steering
- inject vector throttle and brake values for a better RWD experience, mapped to right analog stick up and down

### Usage

- load prx with game, see https://www.ppsspp.org/docs/reference/plugins/ for ppsspp
- be sure to map right analog stick directions in ppsspp settings, note that it is possible to map analog triggers to them
- the hooking code may or may not work with a vita, don't have one to test, refer to how one can load psp prx plugins over there
- camera rotation can additionally be enabled by creating `ms0:/PSP/GTRemastered_camera_controls.txt`, binds to right analog left and right

### Compability
- EU v2.00 (UCES01245 2.00)
- US v2.00 (UCUS98632 2.00)
- JP v1.01 (UCJS10100 1.01)
- ASIA v1.00 (UCAS40265 1.00)

### Hooking references

- https://github.com/TheOfficialFloW/RemasteredControls
- https://github.com/albe/joysens
- https://github.com/Freakler/ppsspp-GTARemastered

### TODO

- finish real HW hooking when I have a psp again
