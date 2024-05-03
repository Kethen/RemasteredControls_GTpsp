#ifndef _HOOKING_
#define _HOOKING_
#include <logging.h>

#include <psputils.h>
#include <pspsdk.h>
#include <psploadcore.h>

u32 MakeSyscallStub(void *function);

#define MAKE_JUMP(a, f) _sw(0x08000000 | (((u32)(f) & 0x0FFFFFFC) >> 2), a);

#define GET_JUMP_TARGET(x) (0x80000000 | (((x) & 0x03FFFFFF) << 2))

u32 offset_digital_to_analog = 0;
u32 offset_populate_car_digital_control = 0;
u32 offset_populate_car_analog_control = 0;

#define HIJACK_FUNCTION(a, f, ptr) \
{ \
	LOG("hijacking function at 0x%lx with 0x%lx", (u32)a, (u32)f); \
	u32 _func_ = (u32)a; \
	u32 ff = (u32)f; \
	int _interrupts = pspSdkDisableInterrupts(); \
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
	sceKernelDcacheWritebackAll(); \
	sceKernelIcacheClearAll(); \
	pspSdkEnableInterrupts(_interrupts); \
	LOG("original instructions: 0x%lx 0x%lx", _lw((u32)patch_buffer), _lw((u32)patch_buffer + 8)); \
}

// XXX ppsspp loading savestate reloads module imports and overwrites this kind of hooking in case HLE
// syscall changed
// https://github.com/hrydgard/ppsspp/blob/master/Core/HLE/sceKernelModule.cpp
// if this kind of hooking on ppsspp cannot be avoided, repatch in a slow thread loop maybe, at least peek
// can be used instead for this particular plugin

// jacking JR_SYSCALL in ppsspp, so just save the two instructions, instead of seeking the target
// also scan other modules for the same pattern and patch them if ppsspp
// for real hw, go the jump target then attempt the more standard two instructions hijack
// hopefully works with the static args loaded sceCtrl functions, at least referencing uofw and joysens
#define HIJACK_SYSCALL_STUB(a, f, ptr) \
{ \
	LOG("hijacking syscall stub at 0x%lx with 0x%lx", (u32)a, (u32)f); \
	u32 _func_ = (u32)a; \
	u32 ff = (u32)f; \
	int _interrupts = pspSdkDisableInterrupts(); \
	if(!is_emulator){ \
		_func_ = GET_JUMP_TARGET(_lw(_func_)); \
	} \
	u32 pattern[2]; \
	_sw(_lw(_func_), (u32)pattern); \
	_sw(_lw(_func_ + 4), (u32)pattern + 4); \
	static u32 patch_buffer[3]; \
	if(is_emulator){ \
		_sw(_lw((u32)pattern), (u32)patch_buffer); \
		_sw(_lw((u32)pattern + 4), (u32)patch_buffer + 4); \
	}else{ \
		_sw(_lw((u32)pattern), (u32)patch_buffer); \
		_sw(_lw((u32)pattern + 4), (u32)patch_buffer + 8); \
		MAKE_JUMP((u32)patch_buffer + 4, _func_ + 8); \
	} \
	_sw(0x08000000 | (((u32)(ff) >> 2) & 0x03FFFFFF), _func_); \
	_sw(0, _func_ + 4); \
	ptr = (void *)patch_buffer; \
	sceKernelDcacheWritebackAll(); \
	sceKernelIcacheClearAll(); \
	pspSdkEnableInterrupts(_interrupts); \
	if(!is_emulator){ \
		LOG("real hardware mode, retargetting function 0x%lx", _func_); \
	} \
	LOG("original instructions: 0x%lx 0x%lx", _lw((u32)pattern), _lw((u32)pattern + 4)); \
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
				LOG("scanning module %s in ppsspp mode", info.name); \
				LOG("info.text_addr: 0x%x info.text_size: 0x%x info.nsegment: 0x%x", info.text_addr, info.text_size, (int)info.nsegment); \
				u32 j; \
				for(j = 0;j < info.nsegment; j++){ \
					LOG("info.segmentaddr[%ld]: 0x%x info.segmentsize[%ld]: 0x%x", j, info.segmentaddr[j], j, info.segmentsize[j]); \
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
						LOG("found instruction pattern 0x%lx 0x%lx at 0x%lx, patching", pattern[0], pattern[1], addr); \
						_interrupts = pspSdkDisableInterrupts(); \
						_sw(0x08000000 | (((u32)(ff) >> 2) & 0x03FFFFFF), addr); \
						_sw(0, addr + 4); \
						sceKernelDcacheWritebackAll(); \
						sceKernelIcacheClearAll(); \
						pspSdkEnableInterrupts(_interrupts); \
					} \
				} \
			} \
		} \
	} \
}

#endif
