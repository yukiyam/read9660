#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "isopath.h"


void PathInit(PathStore *ps)
{
	ps->buf = NULL;
	ps->curlen = 0;
}

void PathDestroy(PathStore *ps)
{
	free(ps->buf);
	ps->buf = NULL;
	ps->curlen = 0;
}

int PathAppend1(PathStore *ps, const char *name, long namelen)
{
	char *p;
	if (namelen > 0) {
		p = realloc(ps->buf, ps->curlen + namelen + 1);
		if (p) {
			ps->buf = p;
			memmove(&p[ps->curlen], name, namelen);
			p[ps->curlen+namelen] = '\0';
			ps->curlen += namelen + 1;
			return 0;
		}
		else {
			return -1;
		}
	}
	else
		return -1;
}

void PathRemove(PathStore *ps, int count)
{
	while (count-- > 0 && ps->curlen > 0) {
		long idx = ps->curlen - 1;
		while (idx > 0 && ps->buf[idx-1] != 0)
			idx--;
		ps->curlen = idx;
	}
}

long PathLength(PathStore *ps, const char *sep)
{
	long pos = 0;
	long count = 0;
	while (pos < ps->curlen) {
		long n = strlen(&ps->buf[pos]);
		pos += n+1;
		count++;
	}
	return ps->curlen - 1 + (strlen(sep) - 1) * count;
}

void PathGet(PathStore *ps, const char *sep, char *outbuf, long bufsize)
{
	long pos = 0;
	long pos2 = 0;
	long seplen = strlen(sep);
	while (pos < ps->curlen && pos2 < bufsize) {
		long n = strlen(&ps->buf[pos]);
		long copy = n;
		if (pos2 + copy > bufsize)
			copy = bufsize - pos2;
		memmove(&outbuf[pos2], &ps->buf[pos], copy);
		pos += n+1;
		pos2 += copy;
		if (pos < ps->curlen) {
			copy = seplen;
			if (pos2 + copy > bufsize)
				copy = bufsize - pos2;
			memmove(&outbuf[pos2], sep, copy);
			pos2 += copy;
		}
	}
	if (pos2 < bufsize)
		outbuf[pos2] = 0;


}

void PathClear(PathStore *ps)
{
	free(ps->buf);
	ps->buf = NULL;
	ps->curlen = 0;
}

