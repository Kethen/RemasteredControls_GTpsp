### Game tweaks implemented in CWCheat codes

- Wider fov __
	- Default FOVs are bumper:33 cockpit:52.xx roof:35 follow:33, the code forces all fov to it's indicated value
	- "Disable Culling" introduced in https://github.com/hrydgard/ppsspp/pull/18572 in graphic settings speed hacks can remove most of the visible popping on the side of the screen, it is available on latest git builds, and should be available on release builds when 1.17 comes
- Force racing line on/off
	- Useful when multiple players want different racing/driving line setting, or when one wants to toggle racing/driving line mid race
- Break jackpot event to avoid music change from jackpot race
	- I didn't find precisly where a jackpot event in multiplayer race would change music, this only prevents music change by putting the event in a weird state
	- Whether a race is a jackpot race or not and the choosen player is only shown at the end of the race, on the race reward screen
	- Because the event is stuck in a weird state, the bottom right icon would stay on screen even if the race turns out to not be a jackpot race
