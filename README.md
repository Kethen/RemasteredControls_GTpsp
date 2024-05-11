### Remastered Controls for GTpsp

- override internal steering deadzone for fine steering
- inject vector throttle and brake values for a better RWD experience

### Usage

- load prx with game, see https://www.ppsspp.org/docs/reference/plugins/ for PPSSPP, see https://consolemods.org/wiki/Vita:Adrenaline#Adding_Plugins for PSVita and https://www.reddit.com/r/PSP/wiki/plugins/ for PSP

### Keybinds

- on PPSSPP, throttle is bound to right stick left, brake is bound to right stick down, remap right stick left and down in ppsspp accordingly to your desired throttle and brake control
	- on windows, please use version 1.17 and up
- camera rotation on bumper and cockpit view can additionally be enabled on PPSSPP through the settings file
	- binds to left stick up and down when enabled, remap PPSSPP left stick up and down to your desired camera control inputs accordingly
- on PSVita, throttle is bound to right stick up, brake is bound to right stick down
- on PSP, it only reduces steering deadzones, there is no keybinding

### Settings

- one can write `ms0:/PSP/GTRemastered_settings.txt` or `ef0:/PSP/GTRemastered_settings.txt` to modify the plugin's behavior
```
<camera control on PPSSPP on/off> <inner analog deadzone 0-127> <outer analog deadzone 0-127>
```

- eg. enable camera control on PPSSPP, map the start of analog input to 10/127, end of analog input to 117/127
```
1 10 117
```

### Compability
- EU v2.00 (UCES01245 2.00)
- US v2.00 (UCUS98632 2.00)
- JP v1.01 (UCJS10100 1.01)
- ASIA v1.00 (UCAS40265 1.00)

### Hooking references

- https://github.com/TheOfficialFloW/RemasteredControls
- https://github.com/albe/joysens
- https://github.com/Freakler/ppsspp-GTARemastered

### Credits

- https://github.com/kotcrab/ghidra-allegrex for making psp games modding easier
- https://github.com/pspdev , m33 and pro cfw for psp homebrew development tools
- https://github.com/hrydgard/ppsspp for an awesome hle psp

### Extra game tweaking CWCheat codes

- check the game_tweaks directory of this repository
