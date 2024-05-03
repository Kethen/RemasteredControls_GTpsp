#ifndef _HOOKING_
#define _HOOKING_
#include "logging.h"

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
	u32 _ff = (u32)f; \
	int _interrupts = pspSdkDisableInterrupts(); \
	sceKernelDcacheWritebackInvalidateAll(); \
	if(!is_emulator){ \
		_ff = MakeSyscallStub(f); \
	} \
	static u32 patch_buffer[3]; \
	_sw(_lw(_func_), (u32)patch_buffer); \
	_sw(_lw(_func_ + 4), (u32)patch_buffer + 8);\
	MAKE_JUMP((u32)patch_buffer + 4, _func_ + 8); \
	_sw(0x08000000 | (((u32)(_ff) >> 2) & 0x03FFFFFF), _func_); \
	_sw(0, _func_ + 4); \
	ptr = (void *)patch_buffer; \
	sceKernelDcacheWritebackInvalidateAll(); \
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
	u32 _ff = (u32)f; \
	int _interrupts = pspSdkDisableInterrupts(); \
	sceKernelDcacheWritebackInvalidateAll(); \
	if(!is_emulator){ \
		_func_ = GET_JUMP_TARGET(_lw(_func_)); \
	} \
	u32 _pattern[2]; \
	_sw(_lw(_func_), (u32)_pattern); \
	_sw(_lw(_func_ + 4), (u32)_pattern + 4); \
	static u32 patch_buffer[3]; \
	if(is_emulator){ \
		_sw(_lw(_func_), (u32)patch_buffer); \
		_sw(_lw(_func_ + 4), (u32)patch_buffer + 4); \
	}else{ \
		_sw(_lw(_func_), (u32)patch_buffer); \
		_sw(_lw(_func_ + 4), (u32)patch_buffer + 8); \
		MAKE_JUMP((u32)patch_buffer + 4, _func_ + 8); \
	} \
	_sw(0x08000000 | (((u32)(_ff) >> 2) & 0x03FFFFFF), _func_); \
	_sw(0, _func_ + 4); \
	ptr = (void *)patch_buffer; \
	sceKernelDcacheWritebackInvalidateAll(); \
	sceKernelIcacheClearAll(); \
	pspSdkEnableInterrupts(_interrupts); \
	if(!is_emulator){ \
		LOG("real hardware mode, retargetting function 0x%lx", _func_); \
	} \
	LOG("original instructions: 0x%lx 0x%lx", _lw((u32)_pattern), _lw((u32)_pattern + 4)); \
	if(is_emulator){ \
		SceUID _modules[32]; \
		static u32 _scan_cache[32] = {0}; \
		SceKernelModuleInfo _info; \
		int _i, _count = 0; \
		if (sceKernelGetModuleIdList(_modules, sizeof(_modules), &_count) >= 0) { \
			for (_i = 0; _i < _count; _i++) { \
				if(_scan_cache[_i] != 0){ \
					continue; \
				} \
				_info.size = sizeof(SceKernelModuleInfo); \
				if (sceKernelQueryModuleInfo(_modules[_i], &_info) < 0) { \
					continue; \
				} \
				if (strcmp(_info.name, MODULE_NAME) == 0) { \
					continue; \
				} \
				LOG("scanning module %s in ppsspp mode", _info.name); \
				LOG("info.text_addr: 0x%x info.text_size: 0x%x info.nsegment: 0x%x", _info.text_addr, _info.text_size, (int)_info.nsegment); \
				u32 _j; \
				for(_j = 0;_j < _info.nsegment; _j++){ \
					LOG("info.segmentaddr[%ld]: 0x%x info.segmentsize[%ld]: 0x%x", _j, _info.segmentaddr[_j], _j, _info.segmentsize[_j]); \
				} \
				if(_info.text_size == 0){ \
					if(_info.nsegment >= 1 && _info.segmentaddr[0] == _info.text_addr){ \
						_info.text_size = _info.segmentsize[0]; \
					} \
				} \
				for(_j = 0; _j < _info.text_size; _j+=4){ \
					u32 _addr = _j + _info.text_addr; \
					int _found = 0; \
					_interrupts = pspSdkDisableInterrupts(); \
					if(_lw((u32)_pattern) == _lw(_addr + 0) && _lw((u32)_pattern + 4) == _lw(_addr + 4)){ \
						_scan_cache[_i] = _addr; \
						_found = 1; \
					} \
					pspSdkEnableInterrupts(_interrupts); \
					if(_found){ \
						continue; \
					} \
				} \
			} \
			for(_i = 0; _i < _count; _i++){ \
				u32 _addr = _scan_cache[_i]; \
				if(_addr != 0){ \
					int _patched = 0; \
					_interrupts = pspSdkDisableInterrupts(); \
					if(_lw((u32)_pattern) == _lw(_addr + 0) && _lw((u32)_pattern + 4) == _lw(_addr + 4)){ \
						_sw(0x08000000 | (((u32)(_ff) >> 2) & 0x03FFFFFF), _addr); \
						_sw(0, _addr + 4); \
						_patched = 1; \
						sceKernelIcacheClearAll(); \
					} \
					pspSdkEnableInterrupts(_interrupts); \
					if(_patched){ \
						LOG("found instruction pattern 0x%lx 0x%lx at 0x%lx, patching", _pattern[0], _pattern[1], _addr); \
					} \
				} \
			} \
		} \
	} \
}

#endif
