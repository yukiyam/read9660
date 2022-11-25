#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

#include "isopath.h"

#define LE_CPU 1

/* ISO 9660 info -> https://wiki.osdev.org/ISO_9660 */

struct ImageInfo_ {
	FILE *fp;
	int sectorsize;
	int modeoff;
	uint32_t startaddr;
	
	int vdend;
	uint32_t rootaddr;
	uint32_t rootlen;
	uint32_t mpathtableaddr;
};
typedef struct ImageInfo_ ImageInfo;


static uint32_t Get32BE(const void *mem, long off)
{
	const uint8_t *p = mem;
	p += off;
	return p[0]*16777216 + p[1]*65536 + p[2]*256 + p[3];
}
static uint32_t Get32LE(const void *mem, long off)
{
	const uint8_t *p = mem;
	p += off;
	return p[0] + p[1]*256 + p[2]*65536 + p[3]*16777216;
}
static uint16_t Get16BE(const void *mem, long off)
{
	const uint8_t *p = mem;
	p += off;
	return p[0]*256 + p[1];
}
static uint16_t Get16LE(const void *mem, long off)
{
	const uint8_t *p = mem;
	p += off;
	return p[0] + p[1]*256;
}

static uint32_t Get32LEBE(const void *mem, long off)
{
	return Get32BE(mem, off+4);
}

static uint16_t Get16LEBE(const void *mem, long off)
{
	return Get16BE(mem, off+2);
}

void Error(const char *fmt, ...)
{
	va_list ap;
	fputs("error: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputs("\n", stderr);
}
void Msg(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputs("\n", stderr);
}


char * DateStr(const char datetime[17])
{
	static char datebuf[32];
	signed char td = datetime[16];
	int tdhm;
	memmove(datebuf, datetime, 16);
	tdhm = abs(td) / 4 * 100;
	tdhm += abs(td) % 4 * 15;
	if (td < 0) tdhm = - tdhm;
	sprintf(&datebuf[16], "%+05d", tdhm);
	return datebuf;
}

int LoadSector(const ImageInfo *ii, int lba, char mem[2048])
{
	int r;
	long c;
	r = fseek(ii->fp, ii->sectorsize * (lba) + ii->modeoff, SEEK_SET);
	if (r != 0)
		return -1;
	c = fread(mem, 1, 2048, ii->fp);
	if (c != 2048)
		return -1;
	return 0;
}

int DumpPrimaryVolumeDescriptor(const char sector[2048])
{
	if (memcmp(&sector[0], "\1CD001\1", 7) != 0){
		Error("not a volume descriptor");
		return -1;
	}
	
	printf("System ID: '%.32s'\n", &sector[8]);
	printf("Volume ID: '%.32s'\n", &sector[40]);
	
	printf("Volume Space Size: %d\n", Get32LEBE(sector, 80));
	
	printf("Volume Set Size : %d\n", Get16LEBE(sector, 120));
	printf("Volume Sequence #: %d\n", Get16LEBE(sector, 124));
	printf("Logical Block Size: %d\n", Get16LEBE(sector, 128));
	printf("Path Table Size: %d\n", Get32LEBE(sector, 132));
	printf("Location of Type-L Path Table: %d\n", Get32LE(sector, 140));
	printf("Location of Optional Type-L Path Table: %d\n", Get32LE(sector, 144));
	printf("Location of Type-M Path Table: %d\n", Get32BE(sector, 148));
	printf("Location of Optional Type-M Path Table: %d\n", Get32BE(sector, 152));
	// 156-190 - directory entry for root directory
	printf("Location of Root Directory Extent: %d\n", Get32LEBE(sector, 158));
	printf("Data Length of Root Directory: %d\n", Get32LEBE(sector, 166));
	
	printf("Volume Set ID: '%.128s'\n", &sector[190]);
	printf("Publisher ID: '%.128s'\n", &sector[318]);
	printf("Data Preparer ID: '%.128s'\n", &sector[446]);
	printf("Application ID: '%.128s'\n", &sector[574]);
	printf("Copyright File ID: '%.37s'\n", &sector[702]);
	printf("Abstract File ID: '%.37s'\n", &sector[739]);
	printf("Bibliographic File ID: '%.37s'\n", &sector[776]);
	printf("Volume Creation Date/Time: '%s'\n", DateStr(&sector[813]));
	printf("Volume Modification Date/Time: '%s'\n", DateStr(&sector[830]));
	printf("Volume Expiration Date/Time: '%s'\n", DateStr(&sector[847]));
	printf("Volume Effective Date/Time: '%s'\n", DateStr(&sector[864]));
	
	return 0;
}


int DetectMode(FILE *fp, ImageInfo *ii)
{
	int r;
	char buf[2352];
	
	ii->fp = fp;
	ii->startaddr = 0;
	
	r = fseek(fp, 16 * 2048, SEEK_SET);
	fread(buf, 1, 2048, fp);
	if (r == 0 && memcmp(&buf[1], "CD001", 5) == 0) {
		ii->sectorsize = 2048;
		ii->modeoff = 0;
		Msg("Mode 2048");
		return 0;
	}
	else {
		r = fseek(fp, 16 * 2352, SEEK_SET);
		fread(buf, 1, 2352, fp);
		if (r == 0 && memcmp(&buf[16+1], "CD001", 5) == 0) {
			ii->sectorsize = 2352;
			ii->modeoff = 16;
			Msg("Mode 2352/16");
			return 0;
		}
		else if (r == 0 && memcmp(&buf[24+1], "CD001", 5) == 0) {
			ii->sectorsize = 2352;
			ii->modeoff = 24;
			Msg("Mode 2352/24");
			return 0;
		}
	}
	
	// detection fails
	Error("could not locate the volume descriptors");
	ii->sectorsize = 0;
	ii->modeoff = 0;
	return -1;
}


int DoVolumeDescriptors(ImageInfo *ii)
{
	char buf[2048];
	int i;
	int primary = 0;
	int r;
	
	for (i = 16; ; i++) {
		r = LoadSector(ii, i, buf);
		if (r != 0)
			break;
		Msg("Volume descriptor at %d: %d", i, buf[0]);
		if (buf[0] == '\1') {
			DumpPrimaryVolumeDescriptor(buf);
			primary = i;
			ii->rootaddr = Get32LEBE(buf, 158);	// this makes the key to find the path table
			ii->rootlen = Get32LEBE(buf, 166);
			ii->mpathtableaddr = Get32BE(buf, 148);
		}
		else if (buf[0] == '\377') {
			ii->vdend = i;
			break;
		}
	}
	if (primary == 0) {
		Error("could not find the primary volume descriptor");
		return -1;
	}
	return 0;
}

int LooksLikePathTable(ImageInfo *ii, char buf[2048])
{
	return buf[0] == 1 && Get32BE(buf, 2) == ii->rootaddr && Get16BE(buf, 6) == 1 && buf[8] == 0;
}

int FindPathTable(ImageInfo *ii)
{
	int i;
	int r;
	char buf[2048];
	int found = 0;
	
	r = LoadSector(ii, ii->mpathtableaddr, buf);
	if (r == 0 && LooksLikePathTable(ii, buf)) {
		found = 1;
		Msg("Path table found at %d, assuming base offset zero.", ii->mpathtableaddr);
	}
	else {
		Msg("Searching for the path table...");
		for (i = ii->vdend; ; i++) {
			r = LoadSector(ii, i, buf);
			if (r != 0)
				break;
			if (LooksLikePathTable(ii, buf)) {
				// found
				Msg("Found.");
				ii->startaddr = ii->mpathtableaddr - i;
				Msg("Path table found at sector %d, assuming base offset %d.", i, ii->startaddr);
				found = 1;
			}
		}
	}
	
	if (! found) {
		Error("could not find the path table");
	}
	
	return 0;
}

char * FlagStr(int flags)
{
	char *flgstr = "HDAXM--+";
	static char flgbuf[9];
	int i;
	for (i = 0; i < 8; i++) {
		if (flags & (1 << i))
			flgbuf[i] = flgstr[i];
		else
			flgbuf[i] = '-'; 
	}
	return flgbuf;
}

char * XAFlagStr(int flags)
{
	char *flagstr = "rwx-rwx-rwx12iad";
	static char flagbuf[17];
	int i;
	for (i = 0; i < 16; i++) {
		if (flags & (1 << i))
			flagbuf[i] = flagstr[i];
		else
			flagbuf[i] = '-';
	}
	return flagbuf;
}

void Indent(int count, FILE *fp)
{
	int i;
	for (i = 0; i < count; i++)
		fputs("  ", fp);
}

int DumpDirectory(ImageInfo *ii, int indent, PathStore *ps, uint32_t addr, uint32_t len)
{
	char buf[2048];
	char pathbuf[512];
	int sec;
	int r;
	int ent;
	
	PathGet(ps, "/", pathbuf, 512);
	pathbuf[511] = 0;
	printf("/%s\n\n", pathbuf);
	
	for (sec = 0; sec < len; sec += 2048) {
		r = LoadSector(ii, addr + sec - ii->startaddr, buf);
		ent = 0;
		do {
			int reclen = buf[ent] & 255;
			int namelen = buf[ent+32];
			int sysoff = 33 + namelen + ! (namelen & 1);
			uint32_t loc = Get32LEBE(buf, ent+2);
			uint32_t len = Get32LEBE(buf, ent+10);
			int flags = buf[ent+25];
			if (reclen == 0)
				break;
			if (namelen == 1 && buf[ent+33] == '\0') {
				//Indent(indent, stdout);
				//printf(".\n");
			}
			else if (namelen == 1 && buf[ent+33] == '\1') {
				//Indent(indent, stdout);
				//printf("..\n");
			}
			else {
				Indent(indent, stdout);
				printf("%.*s", namelen, &buf[ent+33]);
				printf(" (addr %d, size %d, %s)", loc, len, FlagStr(flags));
				if (reclen >= sysoff + 14) {
					if (Get16BE(buf, ent+sysoff+6) == 0x5841) {	// 'XA'
						int xaflags = Get16BE(buf, ent+sysoff+4);
						printf(" XA[%s]", XAFlagStr(xaflags));
					}
				}
				printf("\n");
			}
			
			ent += reclen;
		} while (ent < 2048);
	}
	
	/* 2nd loop: do subdirectories */
	for (sec = 0; sec < len; sec += 2048) {
		r = LoadSector(ii, addr + sec - ii->startaddr, buf);
		ent = 0;
		do {
			int reclen = buf[ent] & 255;
			int namelen = buf[ent+32];
			int sysoff = 33 + namelen + ! (namelen & 1);
			uint32_t loc = Get32LEBE(buf, ent+2);
			uint32_t len = Get32LEBE(buf, ent+10);
			int flags = buf[ent+25];
			if (reclen == 0)
				break;
			if (namelen == 1 && buf[ent+33] == '\0') {
				// .
			}
			else if (namelen == 1 && buf[ent+33] == '\1') {
				// ..
			}
			else {
				if (flags & 0x02) {
					// directory
					PathAppend1(ps, &buf[ent+33], namelen);
					printf("\n");
					DumpDirectory(ii, indent, ps, loc, len);
					PathRemove(ps, 1);
				}
			}
			
			ent += reclen;
		} while (ent < 2048);
	}
	
	return 0;
}

int main(int argc, char *argv[])
{
	FILE *fp;
	ImageInfo iir;
	char buf[2352];
	
	int r = -1;
	if (argc == 2) {
		fp = fopen(argv[1], "rb");
		if (fp) {
			r = DetectMode(fp, &iir);
			if (r == 0)
				r = DoVolumeDescriptors(&iir);
			if (r == 0)
				r = FindPathTable(&iir);
			if (r == 0) {
				PathStore pss;
				PathInit(&pss);
				r = DumpDirectory(&iir, 0, &pss, iir.rootaddr, iir.rootlen);
				PathDestroy(&pss);
			}
		}
		fclose(fp);
		return r == 0 ? 0 : 1;
	}
	else {
		Error("usage: readiso <isofile>");
		return 1;
	}
}

