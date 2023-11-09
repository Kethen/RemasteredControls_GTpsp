/*
  Remastered Controls: analog to digital
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

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <pspthreadman.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <systemctrl.h>

#define MODULE_NAME "GTRemastered"
#define GAME_MODULE_NAME "PDIAPP"

PSP_MODULE_INFO(MODULE_NAME, 0x1007, 1, 0);

int sceKernelQuerySystemCall(void *function);

#define EMULATOR_DEVCTL__IS_EMULATOR     0x00000003

static STMOD_HANDLER previous;

static int is_emulator;
static u32 game_base_addr = 0;

static int override_accel = 0;
static float accel_override = 0;
static int override_brake = 0;
static float brake_override = 0;
static int override_steering = 0;
static short int steering_override = 0;

static unsigned char outer_deadzone = 110;
static unsigned char inner_deadzone = 10;

static u32 MakeSyscallStub(void *function) {
  SceUID block_id = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, "", PSP_SMEM_High, 2 * sizeof(u32), NULL);
  u32 stub = (u32)sceKernelGetBlockHeadAddr(block_id);
  _sw(0x03E00008, stub);
  _sw(0x0000000C | (sceKernelQuerySystemCall(function) << 6), stub + 4);
  return stub;
}

// is there a flush..? or the non async version always syncs?
#define DEBUG 1
#if DEBUG
static int logfd;
#define LOG(...) \
if(logfd > 0){ \
	char logbuf[128]; \
	int loglen = sprintf(logbuf, __VA_ARGS__); \
	if(loglen > 0){ \
		sceIoWrite(logfd, logbuf, loglen); \
	} \
}
#else // DEBUG
#define LOG(...)
#endif // DEBUG
#define VERBOSE 0
#if VERBOSE
#define LOG_VERBOSE(...) LOG(__VA_ARGS__)
#else // VERBOSE
#define LOG_VERBOSE(...)
#endif // VERBOSE

#define CONV_LE(addr, dest) { \
	dest = addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24; \
}

#define CONV_LE16(addr, dest) { \
	dest = addr[0] | addr[1] << 8; \
}

#define MAKE_JUMP(a, f) _sw(0x08000000 | (((u32)(f) & 0x0FFFFFFC) >> 2), a);

#define GET_JUMP_TARGET(x) (0x80000000 | (((x) & 0x03FFFFFF) << 2))

u32 offset_digital_to_analog = 0;
u32 offset_populate_car_digital_control = 0;
u32 offset_populate_car_analog_control = 0;

#define HIJACK_FUNCTION(a, f, ptr) \
{ \
  LOG("hijacking function at 0x%lx with 0x%lx\n", (u32)a, (u32)f); \
  u32 _func_ = (u32)a; \
  LOG("original instructions: 0x%lx 0x%lx\n", _lw(_func_), _lw(_func_ + 4)); \
  u32 ff = (u32)f; \
  if(!is_emulator){ \
    ff = MakeSyscallStub(f); \
  } \
  static u32 patch_buffer[3]; \
  _sw(_lw(_func_), (u32)patch_buffer); \
  _sw(_lw(_func_ + 4), (u32)patch_buffer + 8);\
  MAKE_JUMP((u32)patch_buffer + 4, _func_ + 8); \
  _sw(0x08000000 | (((u32)(ff) >> 2) & 0x03FFFFFF), _func_); \
  _sw(0, _func_ + 4); \
  ptr = (void *)patch_buffer; \
}

// jacking JR_SYSCALL in ppsspp, so just save the two instructions, instead of seeking the target
// also scan other modules for the same pattern and patch them if ppsspp
// for real hw, go the jump target then attempt the more standard two instructions hijack
// hopefully works with the static args loaded sceCtrl functions, at least referencing uofw and joysens
#define HIJACK_SYSCALL_STUB(a, f, ptr) \
{ \
  LOG("hijacking syscall stub at 0x%lx with 0x%lx\n", (u32)a, (u32)f); \
  u32 _func_ = (u32)a; \
  LOG("original instructions: 0x%lx 0x%lx\n", _lw(_func_), _lw(_func_ + 4)); \
  u32 pattern[2]; \
  _sw(_lw(_func_), (u32)pattern); \
  _sw(_lw(_func_ + 4), (u32)pattern + 4); \
  u32 ff = (u32)f; \
  if(!is_emulator){ \
    ff = MakeSyscallStub(f); \
    _func_ = GET_JUMP_TARGET(_lw(a)); \
    LOG("real hardware mode, making syscall stub 0x%lx and retargetting function 0x%lx\n", ff, _func_); \
    LOG("original instructions: 0x%lx 0x%lx\n", _lw(_func_), _lw(_func_ + 4)); \
  } \
  static u32 patch_buffer[3]; \
  if(is_emulator){ \
    _sw(_lw(_func_), (u32)patch_buffer); \
    _sw(_lw(_func_ + 4), (u32)patch_buffer + 4); \
  }else{ \
    _sw(_lw(_func_), (u32)patch_buffer); \
    _sw(_lw(_func_ + 4), (u32)patch_buffer + 8); \
    MAKE_JUMP((u32)patch_buffer + 4, _func_ + 8); \
  } \
  _sw(0x08000000 | (((u32)(ff) >> 2) & 0x03FFFFFF), _func_); \
  _sw(0, _func_ + 4); \
  ptr = (void *)patch_buffer; \
  if(is_emulator){ \
    SceUID modules[32]; \
    SceKernelModuleInfo info; \
    int i, count = 0; \
    if (sceKernelGetModuleIdList(modules, sizeof(modules), &count) >= 0) { \
      for (i = 0; i < count; i++) { \
        info.size = sizeof(SceKernelModuleInfo); \
        if (sceKernelQueryModuleInfo(modules[i], &info) < 0) { \
          continue; \
        } \
        if (strcmp(info.name, MODULE_NAME) == 0) { \
          continue; \
        } \
        LOG("scanning module %s in ppsspp mode\n", info.name); \
        LOG("info.text_addr: 0x%x info.text_size: 0x%x info.nsegment: 0x%x\n", info.text_addr, info.text_size, (int)info.nsegment); \
        u32 j; \
        for(j = 0;j < info.nsegment; j++){ \
          LOG("info.segmentaddr[%ld]: 0x%x info.segmentsize[%ld]: 0x%x\n", j, info.segmentaddr[j], j, info.segmentsize[j]); \
        } \
        if(info.text_size == 0){ \
          if(info.nsegment >= 1 && info.segmentaddr[0] == info.text_addr){ \
            info.text_size = info.segmentsize[0]; \
          } \
        } \
        u32 k; \
        for(k = 0; k < info.text_size; k+=4){ \
          u32 addr = k + info.text_addr; \
          if(/*_lw((u32)pattern) == _lw(addr + 0) &&*/ _lw((u32)pattern + 4) == _lw(addr + 4)){ \
            LOG("found instruction pattern 0x%lx 0x%lx at 0x%lx, patching\n", pattern[0], pattern[1], addr); \
            _sw(0x08000000 | (((u32)(ff) >> 2) & 0x03FFFFFF), addr); \
            _sw(0, addr + 4); \
          } \
        } \
      } \
    } \
  } \
}

static int get_disc_id(char *out_buf){
	char *sfo_path = "disc0:/PSP_GAME/PARAM.SFO";
	int fd = sceIoOpen(sfo_path, PSP_O_RDONLY,0);
	if(fd <= 0){
		LOG("cannot open %s for reading\n", sfo_path);
		return -1;
	}

	sceIoLseek(fd, 0x08, PSP_SEEK_SET);
	unsigned char buf[4];
	if(sceIoRead(fd, &buf, 4) != 4){
		sceIoClose(fd);
		LOG("failed reading key table start from sfo\n");
		return -1;
	}
	u32 key_table_start = 0;
	CONV_LE(buf, key_table_start);
	LOG_VERBOSE("key_table_start is %ld\n", key_table_start);

	if(sceIoRead(fd, &buf, 4) != 4){
		sceIoClose(fd);
		LOG("failed reading data table start from sfo\n");
		return -1;
	}
	u32 data_table_start = 0;
	CONV_LE(buf, data_table_start);
	LOG_VERBOSE("data_table_start is %ld\n", data_table_start);

	if(sceIoRead(fd, &buf, 4) != 4){
		sceIoClose(fd);
		LOG("failed reading tables entries from sfo\n");
		return -1;
	}
	u32 tables_entries = 0;
	CONV_LE(buf, tables_entries);
	LOG_VERBOSE("tables_entries is %ld\n", tables_entries);

	int i;
	for(i = 0;i < tables_entries;i++){
		sceIoLseek(fd, 0x14 + i * 0x10, PSP_SEEK_SET);
		if(sceIoRead(fd, &buf, 2) != 2){
			sceIoClose(fd);
			LOG("failed reading key offset from sfo\n");
			return -1;
		}
		u32 key_offset = 0;
		CONV_LE16(buf, key_offset);

		if(sceIoRead(fd, &buf, 2) != 2){
			sceIoClose(fd);
			LOG("failed reading data format from sfo\n");
			return -1;
		}
		u32 data_format = 0;
		CONV_LE16(buf, data_format);

		if(sceIoRead(fd, &buf, 4) != 4){
			sceIoClose(fd);
			LOG("failed reading data len from sfo\n");
			return -1;
		}
		u32 data_len = 0;
		CONV_LE(buf, data_len);

		sceIoLseek(fd, 4, PSP_SEEK_CUR);
		if(sceIoRead(fd, &buf, 4) != 4){
			sceIoClose(fd);
			LOG("failed reading data offset from sfo\n");
			return -1;
		}
		u32 data_offset = 0;
		CONV_LE(buf, data_offset);

		sceIoLseek(fd, key_offset + key_table_start, PSP_SEEK_SET);
		char keybuf[50];
		int j;
		for(j = 0;j < 50;j++){
			if(sceIoRead(fd, &keybuf[j], 1) != 1){
				sceIoClose(fd);
				LOG("failed reading key from sfo\n");
			}
			if(keybuf[j] == 0){
				break;
			}
		}
		LOG_VERBOSE("key is %s\n", keybuf);

		sceIoLseek(fd, data_offset + data_table_start, PSP_SEEK_SET);
		char databuf[data_len];
		for(j = 0;j < data_len; j++){
			if(sceIoRead(fd, &databuf[j], 1) != 1){
				sceIoClose(fd);
				LOG("failed reading data from sfo\n");
			}
		}
		if(data_format == 0x0204){
			LOG_VERBOSE("utf8 data: %s\n", databuf);
		}else{
			LOG_VERBOSE("data is not utf8, not printing\n");
		}

		if(strncmp("DISC_ID", keybuf, 8) == 0){
			strcpy(out_buf, databuf);
			break;
		}
	}

	sceIoClose(fd);
	return 0;
}

static int apply_deadzone(int val){
	if(val < inner_deadzone){
		return 0;
	}
	val = val - inner_deadzone;
	if(val > outer_deadzone){
		val = outer_deadzone;
	}
	return val * 127 / (outer_deadzone - inner_deadzone);
}

static void sample_input(SceCtrlData *pad_data, int count, int negative){
	if(count < 1){
		LOG("count is %d, processing skipped\n", count);
		return;
	}

	LOG_VERBOSE("processing %d buffers in %s mode\n", count, negative? "negative" : "positive");

	int i;
	for(i = 0;i < count; i++){
		int ry = pad_data[i].Rsrv[1];
		int lx = pad_data[i].Lx;
		#if VERBOSE
		u32 timestamp = pad_data->TimeStamp;
		#endif // VERBOSE

		override_brake = 0;
		override_accel = 0;
		override_steering = 0;

		// right
		if(lx > 128){
			int val = lx - 128;
			val = apply_deadzone(val);
			if(val != 0){
				override_steering = 1;
				steering_override = (0x2000 * val / 127) * -1;
			}
		}
		// left
		if(lx < 128){
			int val = 128 - lx;
			if(val == 128){
				val = 127;
			}
			val = apply_deadzone(val);
			if(val != 0){
				override_steering = 1;
				steering_override = 0x2000 * val / 127;
			}
		}

		// down
		if(ry > 128){
			int val = ry - 128;
			val = apply_deadzone(val);
			if(val != 0){
				override_brake = 1;
				brake_override = (float)val / 127.0f;
			}
		}
		// up
		if(ry < 128){
			int val = 128 - ry;
			if(val == 128){
				val = 127;
			}
			val = apply_deadzone(val);
			if(val != 0){
				override_accel = 1;
				accel_override = (float)val / 127.0f;
			}
		}

		LOG_VERBOSE("timestamp: %d rx: %d ry: %d\n", timestamp, rx, ry);
	}
}

static int (*sceCtrlReadBufferPositiveOrig)(SceCtrlData *pad_data, int count);
int sceCtrlReadBufferPositivePatched(SceCtrlData *pad_data, int count){
	int k1 = pspSdkSetK1(0);
	int res = sceCtrlReadBufferPositiveOrig(pad_data, count);

	sample_input(pad_data, res, 0);

	pspSdkSetK1(k1);
	return res;
}

static void log_modules(){
	SceUID modules[32];
	SceKernelModuleInfo info;
	int i, count = 0;

	if (sceKernelGetModuleIdList(modules, sizeof(modules), &count) >= 0) {
		for (i = 0; i < count; ++i) {
			info.size = sizeof(SceKernelModuleInfo);
			if (sceKernelQueryModuleInfo(modules[i], &info) < 0) {
				continue;
			}
			LOG("module #%d: %s\n", i+1, info.name);
		}
	}
}

int set_offsets(char *disc_id){
	LOG("game_base_addr: 0x%lx\n", game_base_addr);
	if(strcmp("UCES01245", disc_id) == 0){
		offset_digital_to_analog = game_base_addr + 0x14eb40;
		offset_populate_car_digital_control = game_base_addr + 0x126b50;
		offset_populate_car_analog_control = game_base_addr + 0x126dec;
		return 0;
	}

	LOG("unknown dics id %s\n", disc_id);
	return -1;
}

static void (*digital_to_analog_orig)(u32 *param_1, u32 *param_2);
void digital_to_analog_patched(u32 *param_1, u32 *param_2){
	float *accel = (float *)((u32)(param_1[2]) + 0x530);
	u32 *accel_as_int = (u32 *)accel;
	float *brake = (float *)((u32)(param_1[2]) + 0x538);
	u32 *brake_as_int = (u32 *)brake;

	//LOG("accel at 0x%lx is %f, brake at 0x%lx is %f\n", (u32)accel, *accel, (u32)brake, *brake);

	digital_to_analog_orig(param_1, param_2);
	if(*accel_as_int != 0 && override_accel){
		*accel = accel_override;
	}

	if(*brake_as_int != 0 && override_brake){
		*brake = brake_override;
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

static void (*populate_car_analog_control_orig)(u32 param_1, int *param_2, unsigned char *param_3, u32 param_4, u32 param_5, unsigned char param_6);
void populate_car_analog_control_patched(u32 param_1, int *param_2, unsigned char *param_3, u32 param_4, u32 param_5, unsigned char param_6){
	short *steering = (short *)(&param_3[4]);

	populate_car_analog_control_orig(param_1, param_2, param_3, param_4, param_5, param_6);

	if(override_steering){
		param_3[1] = (unsigned char)*param_2;
		*steering = steering_override;
	}
}

int main_thread(SceSize args, void *argp){
	LOG("main thread begins\n");

	sceKernelDelayThread(1000 * 1000 * 5);
	LOG("forcing analog sampling mode");
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	char disc_id[50];
	int disc_id_valid = get_disc_id(disc_id) == 0;
	if(disc_id_valid){
		LOG("disc id is %s\n", disc_id);
	}else{
		LOG("cannot find disc id from sfo, aborting\n");
		return -1;
	}

	if(set_offsets(disc_id) != 0){
		LOG("cannot lookup function offsets with disc id, aborting\n");
		return -1;
	}

	if(is_emulator){
		log_modules();
	}

	HIJACK_FUNCTION(offset_digital_to_analog, digital_to_analog_patched, digital_to_analog_orig);
	HIJACK_FUNCTION(offset_populate_car_digital_control, populate_car_digital_control_patched, populate_car_digital_control_orig);
	HIJACK_FUNCTION(offset_populate_car_analog_control, populate_car_analog_control_patched, populate_car_analog_control_orig);

	u32 sceCtrlReadBufferPositive_addr = (u32)sceCtrlReadBufferPositive;

	if(sceCtrlReadBufferPositive_addr == 0){
		LOG("sceCtrlReadBufferPositive_addr is 0, bailing out\n");
		return 1;
	}

	HIJACK_SYSCALL_STUB(sceCtrlReadBufferPositive_addr, sceCtrlReadBufferPositivePatched, sceCtrlReadBufferPositiveOrig);

	sceKernelDcacheWritebackAll();
	sceKernelIcacheClearAll();

	LOG("main thread finishes\n");
	return 0;
}

void init(){
	#if DEBUG
	logfd = sceIoOpen("ms0:/PSP/"MODULE_NAME".log", PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC, 0777);
	#endif

	LOG("module started\n");
	SceUID thid = sceKernelCreateThread("ra2d", main_thread, 0x18, 4*1024, 0, NULL);
	if(thid < 0){
		LOG("failed creating main thread\n")
		return;
	}
	LOG("created thread with thid 0x%x\n", thid);
	sceKernelStartThread(thid, 0, NULL);
	LOG("main thread started\n");
}

int StartPSP(SceModule2 *mod) {
	if(strcmp(mod->modname, GAME_MODULE_NAME) == 0){
		game_base_addr = mod->text_addr;
    	init();
    }

	if (!previous)
	return 0;

	return previous(mod);
}

static void StartPPSSPP() {
	SceUID modules[32];
	SceKernelModuleInfo info;
	int i, count = 0;

	if (sceKernelGetModuleIdList(modules, sizeof(modules), &count) >= 0) {
		for (i = 0; i < count; ++i) {
			info.size = sizeof(SceKernelModuleInfo);
			if (sceKernelQueryModuleInfo(modules[i], &info) < 0) {
				continue;
			}
			if(strcmp(info.name, GAME_MODULE_NAME) == 0){
				game_base_addr = info.text_addr;
				init();
				break;
			}
		}
	}
}

int module_start(SceSize args, void *argp) {
  is_emulator = sceIoDevctl("kemulator:", EMULATOR_DEVCTL__IS_EMULATOR, NULL, 0, NULL, 0) == 0;
  if (is_emulator) {
    // Just scan the modules using normal/official syscalls.
    StartPPSSPP();
  } else {
    previous = sctrlHENSetStartModuleHandler(StartPSP);
  }
  return 0;
}
