#ifndef _LOGGING_
#define _LOGGING_

#include <pspiofilemgr.h>

#include <stdio.h>

#include "common.h"

// is there a flush..? or the non async version always syncs?
#if DEBUG_LOG
static int logfd = -1;
#define LOG(...) {\
	if(logfd < 0){ \
		logfd = sceIoOpen("ms0:/PSP/" MODULE_NAME ".log", PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND, 0777); \
		if(logfd < 0){ \
			logfd = sceIoOpen("ef0:/PSP/" MODULE_NAME ".log", PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND, 0777); \
		} \
	} \
	char _log_buf[128]; \
	int _log_len = sprintf(_log_buf, __VA_ARGS__); \
	_log_buf[_log_len] = '\n'; \
	_log_len++; \
	if(logfd >= 0){ \
		if(_log_len != 0){ \
			sceIoWrite(logfd, _log_buf, _log_len); \
		} \
		sceIoClose(logfd); \
		logfd = -1; \
	}else{ \
		sceIoWrite(2, _log_buf, _log_len); \
	} \
}
#else // DEBUG_LOG
#define LOG(...)
#endif // DEBUG_LOG
#if VERBOSE
#define LOG_VERBOSE(...) LOG(__VA_ARGS__)
#else // VERBOSE
#define LOG_VERBOSE(...)
#endif // VERBOSE

#endif
