#include <pspiofilemgr.h>

#include <string.h>

#include "logging.h"

#define CONV_LE(addr, dest) { \
	dest = addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24; \
}

#define CONV_LE16(addr, dest) { \
	dest = addr[0] | addr[1] << 8; \
}

int get_disc_id_version(char *id_out, char *version_out){
	char sfo_path[] = "disc0:/PSP_GAME/PARAM.SFO";
	int fd = sceIoOpen(sfo_path, PSP_O_RDONLY,0);

	int id_found = 0;
	int version_found = 0;

	if(fd < 0){
		LOG("cannot open %s for reading", sfo_path);
		return -1;
	}

	sceIoLseek(fd, 0x08, PSP_SEEK_SET);
	unsigned char buf[4];
	if(sceIoRead(fd, &buf, 4) != 4){
		sceIoClose(fd);
		LOG("failed reading key table start from sfo");
		return -1;
	}
	u32 key_table_start = 0;
	CONV_LE(buf, key_table_start);
	LOG_VERBOSE("key_table_start is %ld", key_table_start);

	if(sceIoRead(fd, &buf, 4) != 4){
		sceIoClose(fd);
		LOG("failed reading data table start from sfo");
		return -1;
	}
	u32 data_table_start = 0;
	CONV_LE(buf, data_table_start);
	LOG_VERBOSE("data_table_start is %ld", data_table_start);

	if(sceIoRead(fd, &buf, 4) != 4){
		sceIoClose(fd);
		LOG("failed reading tables entries from sfo");
		return -1;
	}
	u32 tables_entries = 0;
	CONV_LE(buf, tables_entries);
	LOG_VERBOSE("tables_entries is %ld", tables_entries);

	u32 i;
	for(i = 0;i < tables_entries;i++){
		sceIoLseek(fd, 0x14 + i * 0x10, PSP_SEEK_SET);
		if(sceIoRead(fd, &buf, 2) != 2){
			sceIoClose(fd);
			LOG("failed reading key offset from sfo");
			return -1;
		}
		u32 key_offset = 0;
		CONV_LE16(buf, key_offset);

		if(sceIoRead(fd, &buf, 2) != 2){
			sceIoClose(fd);
			LOG("failed reading data format from sfo");
			return -1;
		}
		u32 data_format = 0;
		CONV_LE16(buf, data_format);

		if(sceIoRead(fd, &buf, 4) != 4){
			sceIoClose(fd);
			LOG("failed reading data len from sfo");
			return -1;
		}
		u32 data_len = 0;
		CONV_LE(buf, data_len);

		sceIoLseek(fd, 4, PSP_SEEK_CUR);
		if(sceIoRead(fd, &buf, 4) != 4){
			sceIoClose(fd);
			LOG("failed reading data offset from sfo");
			return -1;
		}
		u32 data_offset = 0;
		CONV_LE(buf, data_offset);

		sceIoLseek(fd, key_offset + key_table_start, PSP_SEEK_SET);
		char keybuf[50];
		u32 j;
		for(j = 0;j < 50;j++){
			if(sceIoRead(fd, &keybuf[j], 1) != 1){
				sceIoClose(fd);
				LOG("failed reading key from sfo");
			}
			if(keybuf[j] == 0){
				break;
			}
		}
		LOG_VERBOSE("key is %s", keybuf);

		sceIoLseek(fd, data_offset + data_table_start, PSP_SEEK_SET);
		char databuf[data_len];
		for(j = 0;j < data_len; j++){
			if(sceIoRead(fd, &databuf[j], 1) != 1){
				sceIoClose(fd);
				LOG("failed reading data from sfo");
			}
		}
		if(data_format == 0x0204){
			LOG_VERBOSE("utf8 data: %s", databuf);
		}else{
			LOG_VERBOSE("data is not utf8, not printing");
		}

		if(strncmp("DISC_ID", keybuf, 8) == 0){
			strcpy(id_out, databuf);
			id_found = 1;
		}

		if(strncmp("DISC_VERSION", keybuf, 8) == 0){
			strcpy(version_out, databuf);
			version_found = 1;
		}

		if(version_found && id_found){
			break;
		}
	}

	sceIoClose(fd);
	return version_found && id_found ? 0 : -1;
}
