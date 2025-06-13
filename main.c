/*
  Remastered Controls: RemasteredControls_GTpsp
  Copyright (C) 2018, TheFloW
  Copyright (C) 2023, Katharine Chui

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <pspctrl.h>

#include <string.h>

#include "hooking.h"
#include "disc_ident.h"

#include "logging.h"

#include <systemctrl.h>

#include "common.h"
#define GAME_MODULE_NAME "PDIAPP"

PSP_MODULE_INFO(MODULE_NAME, PSP_MODULE_KERNEL, 1, 0);

#define EMULATOR_DEVCTL__IS_EMULATOR     0x00000003

#ifndef IRSHELL
static STMOD_HANDLER previous;
#endif

static int is_emulator;
static u32 game_base_addr = 0;

static int override_accel = 0;
static int accel_override = 0;
static int override_brake = 0;
static int brake_override = 0;
static int override_steering = 0;
static short int steering_override = 0;
static int override_camera = 0;
static float camera_override = 0;

static unsigned char outer_deadzone = 114;
static unsigned char inner_deadzone = 10;

static int camera_controls = 0;
static int adjacent_axes = 0;

static int apply_deadzone(int val){
	if(val < inner_deadzone){
		return 0;
	}
	int range = outer_deadzone - inner_deadzone;
	val = val - inner_deadzone;
	if(val > range){
		val = range;
	}
	return val * 127 / range;
}

static void sample_input(SceCtrlData *pad_data, int count, int negative){
	if(count < 1){
		LOG("count is %d, processing skipped", count);
		return;
	}

	LOG_VERBOSE("processing %d buffers in %s mode", count, negative? "negative" : "positive");

	// for this game, it probably makes sense to just process the last buffer
	int rx = pad_data[count - 1].Rsrv[0];
	int ry = pad_data[count - 1].Rsrv[1];
	int lx = pad_data[count - 1].Lx;
	int ly = pad_data[count - 1].Ly;

	#if VERBOSE
	u32 timestamp = pad_data[count - 1].TimeStamp;
	#endif // VERBOSE

	// right, left, down, up

	int lxp = 0;
	int lxn = 0;
	int lyp = 0;
	int lyn = 0;
	//int rxp = 0;
	int rxn = 0;
	int ryp = 0;
	int ryn = 0;

	static int right_stick_looks_dead = 1;
	if(right_stick_looks_dead && (rx != 0 || ry != 0)){
		right_stick_looks_dead = 0;
	}

	if(lx < 128){
		lxn = apply_deadzone(128 - lx);
	}
	if(lx > 128){
		lxp = apply_deadzone(lx - 128);
	}
	if(ly < 128){
		lyn = apply_deadzone(128 - ly);
	}
	if(ly > 128){
		lyp = apply_deadzone(ly - 128);
	}
	if(rx < 128){
		rxn = apply_deadzone(128 - rx);
	}
	/*
	if(rx > 128){
		rxp = apply_deadzone(rx - 128);
	}
	*/
	if(ry < 128){
		ryn = apply_deadzone(128 - ry);
	}
	if(ry > 128){
		ryp = apply_deadzone(ry - 128);
	}

	if(adjacent_axes){
		int tmp = ryn;
		ryn = rxn;
		rxn = tmp;
	}

	override_brake = 0;
	override_accel = 0;
	override_steering = 0;
	override_camera = 0;

	if(lxp > 0){
		override_steering = 1;
		steering_override = lxp * (-1);
	}

	if(lxn > 0){
		override_steering = 1;
		steering_override = lxn;
	}

	if(!right_stick_looks_dead){
		if(ryp > 0){
			override_brake = 1;
			brake_override = ryp;
		}

		if(ryn > 0){
			override_accel = 1;
			accel_override = ryn;
		}
	}

	if(camera_controls){
		if(lyn > 0){
			override_camera = 1;
			camera_override = (float)(lyn * -1.5f) / 127.0f;
		}

		if(lyp > 0){
			override_camera = 1;
			camera_override = (float)(lyp * 1.5f) / 127.0f;
		}
	}

	LOG_VERBOSE("timestamp: %lu lx: %d ly: %d rx: %d ry: %d", timestamp, lx, ly, rx, ry);
}

static int (*sceCtrlReadBufferPositiveOrig)(SceCtrlData *pad_data, int count);
int sceCtrlReadBufferPositivePatched(SceCtrlData *pad_data, int count){
	int k1 = pspSdkSetK1(0);
	int res = sceCtrlReadBufferPositiveOrig(pad_data, count);

	sample_input(pad_data, res, 0);

	pspSdkSetK1(k1);
	return res;
}

int set_offsets(char *disc_id, char *disc_version){
	LOG("game_base_addr: 0x%lx", game_base_addr);
	// EU and US v2.00
	if(strcmp("2.00", disc_version) == 0 && (strcmp("UCES01245", disc_id) == 0 || strcmp("UCUS98632", disc_id) == 0)){
		offset_digital_to_analog = game_base_addr + 0x14eb40;
		offset_populate_car_digital_control = game_base_addr + 0x126b50;
		offset_populate_car_analog_control = game_base_addr + 0x126dec;
		return 0;
	}

	// ASIA v1.00
	if(strcmp("1.00", disc_version) == 0 && strcmp("UCAS40265", disc_id) == 0){
		offset_populate_car_analog_control = game_base_addr + 0x126dec;
		return 0;
	}

	// JP v1.01
	if(strcmp("1.01", disc_version) == 0 && strcmp("UCJS10100", disc_id) == 0){
		offset_populate_car_analog_control = game_base_addr + 0x126dd0;
		return 0;
	}

	LOG("unknown dics id %s with version %s", disc_id, disc_version);
	return -1;
}

// this maps way smoother than trying to go through the ps3 path, but then it breaks replay and ghost
static void (*digital_to_analog_orig)(u32 *param_1, u32 *param_2);
void digital_to_analog_patched(u32 *param_1, u32 *param_2){
	float *accel = (float *)((u32)(param_1[2]) + 0x530);
	u32 *accel_as_int = (u32 *)accel;
	float *brake = (float *)((u32)(param_1[2]) + 0x538);
	u32 *brake_as_int = (u32 *)brake;

	//LOG("accel at 0x%lx is %f, brake at 0x%lx is %f", (u32)accel, *accel, (u32)brake, *brake);

	digital_to_analog_orig(param_1, param_2);
	if(*accel_as_int != 0 && override_accel){
		*accel = (float)accel_override / 127.0f;
		if(*accel > 1.0){
			*accel = 1.0;
		}
	}

	if(*brake_as_int != 0 && override_brake){
		*brake = (float)brake_override / 127.0f;
		if(*brake > 1.0){
			*brake = 1.0;
		}
	}
}

static void (*populate_car_digital_control_orig)(unsigned char *param_1, u32 param_2, u32 param_3);
void populate_car_digital_control_patched(unsigned char *param_1, u32 param_2, u32 param_3){
	unsigned short int *accel_control = (unsigned short int *)&param_1[8];
	unsigned short int *brake_control = (unsigned short int *)&param_1[10];

	populate_car_digital_control_orig(param_1, param_2, param_3);

	if(override_accel){
		*accel_control = 1;
		param_1[0] &= 0x9d;
	}
	if(override_brake){
		*brake_control = 1;
		param_1[0] &= 0xfb;
	}
}

// this is naturally invoked when button bound for steering is not pressed, so likely invoked for converting analog stick value to steering
static void (*populate_car_analog_control_orig)(u32 param_1, int *param_2, unsigned char *param_3, u32 param_4, u32 param_5, unsigned char param_6);
void populate_car_analog_control_patched(u32 param_1, int *param_2, unsigned char *param_3, u32 param_4, u32 param_5, unsigned char param_6){
	short *steering = (short *)(&param_3[4]); // +- 0x2000 int
	float *camera_rotation = (float *)(&param_3[0x2c]); // +-1.0 float
	short *throttle = (short *)(&param_3[0x8]);
	short *brake = (short *)(&param_3[0xA]);
	// an analog handbrake
	//short *handbrake = (short *)(&param_3[0xe]);
	//param_3[0] = param_3[0] | 0x10;
	// analog reverse, also has the weird response curve
	//short *reverse = (short *)(&param_3[0xc]);
	//param_3[0] = param_3[0] | 8;

	// no clutch..?

	// populate_car_analog_control_orig(param_1, param_2, param_3, param_4, param_5, param_6);
	param_3[0] = 0;
	param_3[1] = 0;

	int k1 = pspSdkSetK1(0);

	static void *logged_location = NULL;
	if(param_3 != logged_location){
		logged_location = param_3;
		LOG("car control struct is now at 0x%lx", (uint32_t)param_3);
	}

	int mode;
	sceCtrlGetSamplingMode(&mode);
	if(mode != PSP_CTRL_MODE_ANALOG){
		// the mode might be per thread
		LOG_VERBOSE("sceCtrlGetSamplingMode is not analog..? setting it to analog now");
		sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
		return;
	}

	#if 0
	if(is_emulator){
	#else
	{
	#endif
		SceCtrlData pad_data;
		int res = sceCtrlPeekBufferPositive(&pad_data, 1);
		sample_input(&pad_data, res, 0);
	}

	if(override_steering){
		param_3[0] = param_3[0] | 1;
		param_3[1] = param_3[1] | 2;
		*steering = steering_override * 0x2000 / 127;
		LOG_VERBOSE("applying steering override, val is %d, steering is %d", steering_override, *steering);
	}else{
		*steering = 0;
	}

	if(override_accel){
		// weird curve, 0-0.71 is roughly nothing, 0.81 is roughly 0.25 throttle, 0.91 is roughly 0.5 throttle, 0.96 is roughly 0.75 throttle
		param_3[0] = param_3[0] | 2;
		if(accel_override >= 95){
			static short base = (0x1000 * 0.96f);
			static short offset_throttle = (0x1000 * (1.0f - 0.96f) + 1);
			int throttle_segment = accel_override - 95;
			static int range = 127 - 95;
			*throttle = base + throttle_segment * offset_throttle / range;
			if(*throttle > 4096){
				*throttle = 4096;
			}
		}else if(accel_override >= 64){
			static short base = (0x1000 * 0.91f);
			static short offset_throttle = (0x1000 * (0.96f - 0.91f));
			int throttle_segment = accel_override - 64;
			static int range = 94 - 64;
			*throttle = base + throttle_segment * offset_throttle / range;
		}else if(accel_override >= 32){
			static short base = (0x1000 * 0.81f);
			static short offset_throttle = (0x1000 * (0.91f - 0.81f));
			int throttle_segment = accel_override - 32;
			static int range = 63 - 32;
			*throttle = base + throttle_segment * offset_throttle / range;
		}else{
			static short base = (0x1000 * 0.71f);
			static short offset_throttle = (0x1000 * (0.81f - 0.71f));
			int throttle_segment = accel_override;
			static int range = 31;
			*throttle = base + throttle_segment * offset_throttle / range;
		}
		// *throttle = accel_override * 0x1000 / 127;
		LOG_VERBOSE("applying accel override, val is %d, throttle is %d", accel_override, *throttle);
	}

	if(override_brake){
		param_3[0] = param_3[0] | 4;
		*brake = brake_override * 0x1000 / 127;
		LOG_VERBOSE("applying brake override, val is %d, brake is %d", brake_override, *brake);
	}

	if(override_camera){
		*camera_rotation = camera_override;
	}

	pspSdkSetK1(k1);
}

int _atoi(char *buf){
	int i = 0;
	int val = 0;
	int negative = 0;

	if(buf[0] == '-'){
		negative = 1;
		i = 1;
	}

	while(1){
		if(buf[i] == '\0'){
			break;
		}
		val = val * 10;
		val = val + (int)(buf[i] - '0');
		i++;
	}

	if(negative){
		val = val * (-1);
	}

	return val;
}

void parse_config(){
    char *path = "ms0:/PSP/"MODULE_NAME"_settings.txt";
	int fd = sceIoOpen(path, PSP_O_RDONLY, 0);
	if(fd < 0){
		LOG("cannot open %s for reading, trying ef0:/", path);
		path = "ef0:/PSP/"MODULE_NAME"_settings.txt";
		fd = sceIoOpen(path, PSP_O_RDONLY, 0);
		if(fd < 0){
			LOG("cannot open %s for reading either", path);
			return;
    	}
	}

	char buf[128] = {0};
	u32 len = sceIoRead(fd, buf, sizeof(buf));
	if(len < 0){
		LOG("failed reading %s after opening", path);
		return;
	}

	int arg_idx = 0;
	char arg_buf[128] = {0};
	u32 arg_buf_write_head = 0;
	for(u32 i = 0;i <= len; i++){
		if(i < len && buf[i] != ' ' && buf[i] != '\n'){
			arg_buf[arg_buf_write_head] = buf[i];
			arg_buf_write_head++;
		}else{
		    if(arg_buf_write_head == 0){
		        continue;
		    }
			arg_buf[arg_buf_write_head] = '\0';
			if(arg_idx == 0){
				int num = _atoi(arg_buf);
				camera_controls = num && is_emulator;
				if(camera_controls){
				    LOG("enabling camera controls");
				}else{
				    LOG("not enabling camera controls");
				}
			}else if(arg_idx == 1){
				int num = _atoi(arg_buf);
				if(num > 127){
					num = 127;
				}
				if(num < 0){
					num = 0;
				}
				LOG("overriding inner deadzone %d with %d", inner_deadzone, num);
				inner_deadzone = num;
			}else if(arg_idx == 2){
				int num = _atoi(arg_buf);
				if(num > 127){
					num = 127;
				}
				if(num < 0){
					num = 0;
				}
				LOG("overriding outer deadzone %d with %d", outer_deadzone, num);
				outer_deadzone = num;
			}

			arg_buf_write_head = 0;
			arg_idx++;
		}
	}
}

int init(){
	char disc_id[50];
	char disc_version[50];
	int disc_id_valid = get_disc_id_version(disc_id, disc_version) == 0;
	if(disc_id_valid){
		LOG("disc id is %s", disc_id);
		LOG("disc version is %s", disc_version);
	}else{
		LOG("cannot find disc id from sfo, aborting");
		return -1;
	}

	if(set_offsets(disc_id, disc_version) != 0){
		LOG("cannot lookup function offsets with disc id and version, aborting");
		return -1;
	}

	if(is_emulator){
		adjacent_axes = 1;
		outer_deadzone = 124;
		inner_deadzone = 3;
	}
	parse_config();

	//HIJACK_FUNCTION(offset_digital_to_analog, digital_to_analog_patched, digital_to_analog_orig);
	//HIJACK_FUNCTION(offset_populate_car_digital_control, populate_car_digital_control_patched, populate_car_digital_control_orig);
	HIJACK_FUNCTION(offset_populate_car_analog_control, populate_car_analog_control_patched, populate_car_analog_control_orig);

	#if 0
	if(!is_emulator){
		HIJACK_SYSCALL_STUB((u32)sceCtrlReadBufferPositive, sceCtrlReadBufferPositivePatched, sceCtrlReadBufferPositiveOrig);
	}
	#endif

	if(is_emulator){
		sceKernelDelayThread(1000 * 1000 * 5);
		LOG("boosting input sampling on ppsspp");
		sceCtrlSetSamplingCycle(5555);
	}

	LOG("main thread finishes");
	return 0;
}

#ifndef IRSHELL
int StartPSP(SceModule2 *mod) {
	char namebuf[sizeof(mod->modname) + 1] = {0};
	memcpy(namebuf, mod->modname, sizeof(mod->modname));
	LOG("PSP module %s", namebuf);
	if(strcmp(namebuf, GAME_MODULE_NAME) == 0){
		game_base_addr = mod->text_addr;
		if (!is_emulator){
			// XXX oh no
			game_base_addr = game_base_addr + 0x28;
		}
		LOG("GTPSP module %s found, setting base address to 0x%08lx", namebuf, game_base_addr);
		init();
	}

	if (!previous){
		return 0;
	}

	return previous(mod);
}
#else
int irshell_find_module(SceSize args, void *argp){
	int cycles = 0;
	while(cycles < 60){
		SceUID modules[256] = {0};
		SceKernelModuleInfo info = {0};
		int count = 0;
		LOG("%s: scanning through module list", __func__);
		if(sceKernelGetModuleIdList(modules, sizeof(modules), &count) >= 0){
			for(int i = 0;i < count;i++){
				info.size = sizeof(SceKernelModuleInfo);
				if(sceKernelQueryModuleInfo(modules[i], &info) < 0){
					LOG("failed fetching module info for id 0x%08x", modules[i]);
					continue;
				}
				char namebuf[sizeof(info.name) + 1] = {0};
				memcpy(namebuf, info.name, sizeof(info.name));
				if(strcmp(namebuf, GAME_MODULE_NAME) == 0){
					LOG("GTPSP module %s found with text_addr 0x%08x", namebuf, info.text_addr);
					// XXX I guess ppsspp's value is off
					game_base_addr = info.text_addr + 0x28;
				}else{
					LOG("ignoring module %s", namebuf);
				}
			}
			if(game_base_addr != 0){
				init();
				sceKernelExitDeleteThread(0);
				return 0;
			}
		}else{
			LOG("failed fetching module id list");
		}
		LOG("%s: didn't find GTPSP module, trying again in a second", __func__);
		sceKernelDelayThread(1000 * 1000);
		cycles++;
	}
	LOG("%s: giving up, it has been a whole minute", __func__);
	sceKernelExitDeleteThread(0);
	return 0;
}
#endif

static void StartPPSSPP() {
	SceUID modules[256];
	SceKernelModuleInfo info;
	int i, count = 0;

	if (sceKernelGetModuleIdList(modules, sizeof(modules), &count) >= 0) {
		for (i = 0; i < count; ++i) {
			info.size = sizeof(SceKernelModuleInfo);
			if (sceKernelQueryModuleInfo(modules[i], &info) < 0) {
				continue;
			}
			char namebuf[sizeof(info.name) + 1] = {0};
			memcpy(namebuf, info.name, sizeof(info.name));
			LOG("PPSSPP module %s", namebuf);
			if(strcmp(namebuf, GAME_MODULE_NAME) == 0){
				LOG("GTPSP module %s found", namebuf);
				game_base_addr = info.text_addr;
			}
		}
		if(game_base_addr != 0){
			init();
		}
	}
}

int module_start(SceSize args, void *argp){
	#if DEBUG_LOG
	sceIoRemove("ms0:/PSP/"MODULE_NAME".log");
	sceIoRemove("ef0:/PSP/"MODULE_NAME".log");
	#endif
	LOG("module started");

	is_emulator = sceIoDevctl("kemulator:", EMULATOR_DEVCTL__IS_EMULATOR, NULL, 0, NULL, 0) == 0;

	//if (is_emulator) {
	if (0) {
		// Just scan the modules using normal/official syscalls.
		LOG("starting in ppsspp mode");
		StartPPSSPP();
	}else{
		#ifndef IRSHELL
		LOG("starting in psp hen mode");
		previous = sctrlHENSetStartModuleHandler(StartPSP);
		#else
		LOG("starting in psp irshell mode");
		SceUID thid = sceKernelCreateThread("irshell_find_module", irshell_find_module, 0x18, 0x10000, 0, NULL);
		if(thid < 0){
			LOG("failed creating irshell module searching thread");
			return 0;
		}
		sceKernelStartThread(thid, 0, NULL);
		#endif
	}
	return 0;
}

int module_stop(SceSize args, void *argp){
	LOG("attempting to stop this module, but unload is not really implemented...");
	return 0;
}
