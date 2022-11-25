#ifndef ISOPATH_H
#define ISOPATH_H 1


/*
	[internal storage]
	ps->buf: contiguous list of C strings
	(OK because ISO-9660 filenames never contain NULs)
	ps->curlen: total size of above C strings (including all NULs)
*/
struct PathStore_ {
	char *buf;
	long curlen;
};
typedef struct PathStore_ PathStore;


void PathInit(PathStore *ps);
void PathDestroy(PathStore *ps);
int PathAppend1(PathStore *ps, const char *name, long namelen);
void PathRemove(PathStore *ps, int count);
long PathLength(PathStore *ps, const char *sep);
void PathGet(PathStore *ps, const char *sep, char *buf, long bufsize);
void PathClear(PathStore *ps);



#endif
