/* vim: ft=c ff=unix fenc=utf-8
 * file: renca.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#define __USE_BSD
#include <dirent.h>
#include <enca.h>
#include <iconv.h>

#include "renca.h"
#include "opts.h"

bool
wdir_pop (struct wdlist_t *restrict wdlist, char *restrict dirname)
{
	struct wdir_t *dirre;
	if (wdlist->list)
	{
		// copy all with \0 in end of buffer
		memcpy ((void *)dirname, (const void*)wdlist->list->name, NAME_MAX + 1);
		dirre = wdlist->list;
		wdlist->list = wdlist->list->next;
		free (dirre);
		return true;
	}
	return false;
}

bool
wdir_push (struct wdlist_t *restrict wdlist, const char *restrict dirname)
{
	struct wdir_t *node;
	node = calloc (1, sizeof (struct wdir_t));
	if (node)
	{
		memcpy ((void *)node->name, (const void *)dirname, NAME_MAX);
		node->next = wdlist->list;
		wdlist->list = node;
		return true;
	}
	return false;
}

bool
wdir_empty (struct wdlist_t *wdlist)
{
	return (wdlist->list) ? false : true;
}

const char*
process_name (const char *restrict name, struct _opts_t *restrict opts)
{
	EncaEncoding res;
	size_t len = strlen (name);
	//printf ("PN: %s\n", name);
	/* detect encoding */
	enca_set_significant (opts->enca, 2); /* really fail */
	enca_set_filtering (opts->enca, 0);
	enca_set_termination_strictness (opts->enca, 1);
	res = enca_analyse_const (opts->enca, (const unsigned char *)name, len);
	/* convert name to buffer */
	if (enca_charset_is_known (res.charset))
		return enca_charset_name (res.charset, ENCA_NAME_STYLE_ICONV);
	return NULL;
}

bool
process_dir (const char *restrict path, struct _opts_t *restrict opts)
{
	char *dirname;
	struct wdlist_t wdlist;
	size_t path_len = 0;
	char *sd_name[2] = {NULL, NULL};
	DIR *dirp;
	const char *fromcode = opts->from;
	struct dirent *dir_en;
	//printf ("PD: %s\n", path);
	path_len = strlen (path);
	memset (&wdlist, 0, sizeof (struct wdlist_t));
	wdlist._iconv.cd = (iconv_t) -1;
	dirp = opendir (path);
	if (dirp)
	{
		while ((dir_en = readdir (dirp)))
		{
			dirname = dir_en->d_name;
			if (!strcmp (dirname, ".") || !strcmp (dirname, "..") || !strcmp (dirname, ".git"))
				continue;
			do
			{
				if (opts->enca)
				{
					// fail if codepage not detected
					if (!(fromcode = process_name (dirname, opts)))
					{
						fprintf (stderr, "FAIL: %s/%s\n", path, dirname);
						fprintf (stderr, "enca: [%d] %s\n", enca_errno (opts->enca), enca_strerror (opts->enca, enca_errno (opts->enca)));
						break;
						// exception
					}
				}
				// skip ascii and tocode == fromcode
				if (!strcmp (fromcode, "ASCII") || !strcasecmp (opts->to, fromcode)) /* ~_~ */
					break;
				// init converter
				if (wdlist._iconv.cd == (iconv_t)-1
						|| !wdlist._iconv.from
						|| strcmp (wdlist._iconv.from, fromcode))
				{
					if (wdlist._iconv.cd != (iconv_t)-1)
					{
						iconv_close (wdlist._iconv.cd);
						wdlist._iconv.cd = (iconv_t)-1;
					}
					wdlist._iconv.from = fromcode;
					wdlist._iconv.cd = iconv_open (opts->to, fromcode);
					if (wdlist._iconv.cd == (iconv_t)-1)
					{
						fprintf (stderr, "FAIL: %s/%s\n", path, dirname);
						perror ("iconv_open");
						break;
					}
				}
				// alloc buffers
				if (!*sd_name)
				{
					/* alloc buffer for all names in this directory */
					*sd_name = (char *)malloc ((path_len + NAME_BUFSZ) << 1);
					if (!*sd_name)
					{
						/* print filename and break enumerate */
						fprintf (stderr, "FAIL: %s/%s\n", path, dirname);
						perror ("malloc-rename");
						break;
					}
				}
				sd_name[1] = (sd_name[0] + (path_len + NAME_BUFSZ));
				snprintf (sd_name[0], path_len + NAME_BUFSZ, "%s/%s", path, dirname);
				memset (sd_name[1], 0, path_len + NAME_BUFSZ);
				snprintf (sd_name[1], path_len + NAME_BUFSZ, "%s/", path);
				// convert name
				{
					char *_src_p = dirname;
					char *_dst_p = sd_name[1] + path_len + 1;
					size_t _src_len = strlen (dirname);
					size_t _dst_len = NAME_BUFSZ;
					size_t ret;
					ret = iconv (wdlist._iconv.cd,
							&_src_p,
							&_src_len,
							&_dst_p,
							&_dst_len);
					if ((size_t)-1 == ret)
					{
						fprintf (stderr, "FAIL: rename `%s' to `%s'\n", sd_name[0], sd_name[1]);
						perror ("rename-iconv");
					}
					else
					{
						if (rename (sd_name[0], sd_name[1]) == -1)
						{
							fprintf (stderr, "FAIL: rename `%s' to `%s'\n", sd_name[0], sd_name[1]);
							perror ("rename");
							break;
						}
						else
						{
							dirname = sd_name[1] + path_len + 1;
							printf ("RENAME[%s]: `%s' to `%s'\n", fromcode, sd_name[0], sd_name[1]);
						}
					}
				}
			}
			while (false);
			if (dir_en->d_type == DT_DIR)
			{
				if (!wdir_push (&wdlist, dirname))
				{
					fprintf (stderr, "FAIL: %s/%s\n", path, dirname);
					perror ("wdir_push");
				}
			}
		}
		/* close iconv if openned */
		if (wdlist._iconv.cd != (iconv_t) -1)
			iconv_close (wdlist._iconv.cd);
		closedir (dirp);
		/* TODO: process wdlist here */
		if (!wdir_empty (&wdlist))
		{
			if (!*sd_name)
			{
				*sd_name = (char*)malloc (path_len + (NAME_BUFSZ << 1));
				if (!*sd_name)
				{
					fprintf (stderr, "FAIL: %s/*\n", path);
					perror ("malloc-wdir_pop");
				}
				else
				{
					sd_name[1] = sd_name[0] + path_len + NAME_BUFSZ;
				}

			}
			if (*sd_name)
			{
				while (wdir_pop (&wdlist, sd_name[1]))
				{
					snprintf (sd_name[0], path_len + NAME_BUFSZ, "%s/%s", path, sd_name[1]);
					process_dir (sd_name[0], opts);
				}
			}
		}
		if (*sd_name)
			free (*sd_name);
		return true;
	}
	return false;
}

/* opts */

static void*
_set (struct _opts_t *opts, void *cont_data, uint8_t id, size_t argc, char **argv)
{
	switch (id)
	{
		case _SET_FROM:
			strncpy (opts->from, argv[0], _RENCA_BFSZ);
			break;
		case _SET_TO:
			strncpy (opts->to, argv[0], _RENCA_BFSZ);
			break;
		case _SET_DIR:
			strncpy (opts->dir, argv[0], _RENCA_BFSZ);
			break;
		case _SET_LANG:
			strncpy (opts->lang, argv[0], _RENCA_BFSZ);
			break;
	}
	return NULL;
}

static struct opts_list_t opts_list[] =
{
	{"from", _SET_FROM, 0, (opts_callback_t)_set, 1, NULL},
	{"to", _SET_TO, 0, (opts_callback_t)_set, 1, NULL},
	{"on", _SET_DIR, 0, (opts_callback_t)_set, 1, NULL},
	{"lang", _SET_LANG, 0, (opts_callback_t)_set, 1, NULL}
};

/* main */

int
main (int argc, char *argv[])
{
	iconv_t cdt;
	struct _opts_t opts;
	memset (&opts, 0, sizeof (struct _opts_t));
	opts_parse (&opts, opts_list, (size_t)argc - 1, argv + 1);
	/* checks */
	if (!strlen (opts.to))
		snprintf (opts.to, _RENCA_BFSZ, "UTF-8");
	/* set encoding to enca if len (from) is 0 or from == 'enca' */
	if (!strncmp (opts.from, "enca", 5) || !strlen (opts.from))
	{
		/* set default fromcode */
		snprintf (opts.from, _RENCA_BFSZ, "ASCII");
		/* check enca data */
		if (!(opts.enca = enca_analyser_alloc (opts.lang)))
		{
			fprintf (stderr, "opts 'lang' must be content valid two-letter language code\n");
			return EXIT_FAILURE;
		}
		enca_set_threshold (opts.enca, 1.38);
		enca_set_multibyte (opts.enca, 1);
		enca_set_ambiguity (opts.enca, 1);
		enca_set_garbage_test (opts.enca, 1);
	}
	/* check convert */
	if (!strlen (opts.to) || (cdt = iconv_open (opts.to, opts.from)) == (iconv_t)-1)
	{
		fprintf (stderr, "please define a valid iconv's convert destination and source\n");
		return EXIT_FAILURE;
	}
	iconv_close (cdt);
	if (!*opts.dir)
	{
		opts.dir[0] = '.';
		opts.dir[1] = '\0';
	}
	printf ("process on: %s\n", opts.dir);
	if (opts.enca)
		printf ("convert with enca, use language %s, to %s\n", opts.lang, opts.to);
	else
		printf ("convert from: %s to %s\n", opts.from, opts.to);
	if (!process_dir (".", &opts))
		perror (".");
	if (opts.enca)
		enca_analyser_free (opts.enca);
	return EXIT_SUCCESS;
}

