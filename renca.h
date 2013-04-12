/* vim: ft=c ff=unix fenc=utf-8
 * file: renca.h
 */
#ifndef _RENCA_1365527198_H_
#define _RENCA_1365527198_H_

#include <stdlib.h>
#include <stdint.h>

#define _SET_FROM 1
#define _SET_TO   2
#define _SET_DIR  3
#define _SET_LANG 4

#ifndef PATH_MAX
# define PATH_MAX 4096
#endif
#ifndef NAME_MAX
# define NAME_MAX 255
#endif

#define _RENCA_BFSZ 32

#define NAME_BUFSZ (NAME_MAX + 1)
struct drv_iconv_t
{
	iconv_t cd;
	const char *from;
};

struct wdlist_t
{
	size_t count;
	struct wdir_t *list;
	struct drv_iconv_t _iconv;
};

struct wdir_t
{
	char name[NAME_MAX + 1];
	struct wdir_t *next;
};

#define _FLAG_ENCA 1

struct _opts_t
{
	char from[_RENCA_BFSZ + 1];
	char fromforce[_RENCA_BFSZ + 1];
	char to[_RENCA_BFSZ + 1];
	char lang[_RENCA_BFSZ + 1];
	char dir[PATH_MAX + 1];
	EncaAnalyser enca;
};

#endif /* _RENCA_1365527198_H_ */

