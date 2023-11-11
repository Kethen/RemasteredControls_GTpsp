### Remastered Controls for GTpsp

- override internal steering deadzone for fine steering
- inject vector throttle and brake values for a better RWD experience, mapped to right analog stick up and down

### Usage

- load prx with game, see https://www.ppsspp.org/docs/reference/plugins/ for ppsspp
- the hooking code may or may not work with a vita, don't have one to test, refer to how one can load psp prx plugins over there

### Keybinds

- on ppsspp, throttle is bound to right stick left, brake is bound to right stick down, remap right stick left and down in ppsspp accordingly to your desired throttle and brake control
	- a patched windows ppsspp is provided at the moment to remove hardcoded xinput trigger deadzones, https://github.com/hrydgard/ppsspp/issues/18404
- camera rotation on bumper and cockpit view can additionally be enabled on ppsspp by creating `ms0:/PSP/GTRemastered_camera_controls.txt`
	- binds to left stick up and down when enabled, remap ppsspp left stick up and down to your desired camera control buttons accordingly
- on vita (when supported), throttle is bound to right stick up, brake is bound to right stick down


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
