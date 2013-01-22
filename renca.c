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

#ifndef NAME_MAX
# define NAME_MAX 255
#endif
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
process_name (const char *restrict name, EncaAnalyser *restrict enca)
{
	EncaEncoding res;
	size_t len = strlen (name);
	/* detect encoding */
	enca_set_significant (*enca, 2); /* really fail */
	enca_set_filtering (*enca, 0);
	enca_set_termination_strictness (*enca, 1);
	res = enca_analyse_const (*enca, (const unsigned char *)name, len);
	/* convert name to buffer */
	if (enca_charset_is_known (res.charset))
		return enca_charset_name (res.charset, ENCA_NAME_STYLE_ICONV);
	return NULL;
}

bool
process_dir (const char *restrict path, EncaAnalyser *restrict enca, const char *tocode)
{
	struct wdlist_t wdlist;
	size_t path_len = 0;
	char *sd_name[2] = {NULL, NULL};
	DIR *dirp;
	const char *fromcode;
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
			if (!strcmp (dir_en->d_name, ".") || !strcmp (dir_en->d_name, "..") || !strcmp (dir_en->d_name, ".git"))
				continue;
			do
			{
				// fail if codepage not detected
				if (!(fromcode = process_name (dir_en->d_name, enca)))
				{
					fprintf (stderr, "FAIL: %s/%s\n", path, dir_en->d_name);
					fprintf (stderr, "enca: [%d] %s\n", enca_errno (*enca), enca_strerror (*enca, enca_errno (*enca)));
					continue;
					// exception
				}
				// skip ascii and tocode == fromcode
				if (!strcmp (fromcode, "ASCII") || !strcasecmp (tocode, fromcode)) /* ~_~ */
					continue;
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
					wdlist._iconv.cd = iconv_open (tocode, fromcode);
					if (wdlist._iconv.cd == (iconv_t)-1)
					{
						fprintf (stderr, "FAIL: %s/%s\n", path, dir_en->d_name);
						perror ("iconv_open");
						continue;
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
						/*
						fputs ("FAIL: ", stderr);
						fputs (path, stderr);
						fputs ("/", stderr);
						fputs (dir_en->d_name, stderr);
						fputs ("\n", stderr);
						*/
						fprintf (stderr, "FAIL: %s/%s\n", path, dir_en->d_name);
						perror ("malloc-rename");
						continue;
					}
				}
				sd_name[1] = (sd_name[0] + (path_len + NAME_BUFSZ));
				snprintf (sd_name[0], path_len + NAME_BUFSZ, "%s/%s", path, dir_en->d_name);
				snprintf (sd_name[1], path_len + NAME_BUFSZ, "%s/", path);
				// convert name
				{
					char *_src_p = dir_en->d_name;
					char *_dst_p = sd_name[1] + path_len + 1;
					size_t _src_len = strlen (dir_en->d_name);
					size_t _dst_len = NAME_BUFSZ;
					/* TODO: check sizes */
					iconv (wdlist._iconv.cd,
							&_src_p,
							&_src_len,
							&_dst_p,
							&_dst_len);
					//printf ("X: %s (%s)\n", sd_name[1], fromcode);
					continue;
				}
				/* TODO: check exists path */
				if (rename (sd_name[0], sd_name[1]) == -1)
				{
					fprintf (stderr, "FAIL: rename `%s' to `%s'\n", sd_name[0], sd_name[1]);
					perror ("rename");
					continue;
				}
				else
					fprintf (stderr, "RENAME: `%s' to `%s'\n", sd_name[0], sd_name[1]);
			}
			while (false);
			if (dir_en->d_type == DT_DIR)
			{
				if (!wdir_push (&wdlist, dir_en->d_name))
				{
					fprintf (stderr, "FAIL: %s/%s\n", path, dir_en->d_name);
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
					process_dir (sd_name[0], enca, tocode);
				}
			}
		}
		if (*sd_name)
			free (*sd_name);
		return true;
	}
	return false;
}

void
usage (const char *arg)
{
	fprintf (stderr, "Example:\n");
	fprintf (stderr, "\t%s ru UTF-8\n", arg);
}

int
main (int argc, char *argv[])
{
	iconv_t cdt;
	EncaAnalyser enca = NULL;
	if (argc < 3)
	{
		usage (argv[0]);
		return EXIT_FAILURE;
	}
	if ((cdt = iconv_open (argv[2], "UTF-8")) == (iconv_t)-1)
	{
		fprintf (stderr, "please define a valid iconv's converions destination\n");
		usage (argv[0]);
		return EXIT_FAILURE;
	}
	iconv_close (cdt);
	if (!(enca = enca_analyser_alloc (argv[1])))
	{
		fprintf (stderr, "please define two-letter language code in command line\n");
		usage (argv[0]);
		return EXIT_FAILURE;
	}
	enca_set_threshold (enca, 1.38);
	enca_set_multibyte (enca, 1);
	enca_set_ambiguity (enca, 1);
	enca_set_garbage_test (enca, 1);
	if (!process_dir (".", &enca, argv[2]))
		perror (".");
	enca_analyser_free (enca);
	return EXIT_SUCCESS;
}

