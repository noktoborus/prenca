/* vim: ft=c ff=unix fenc=utf-8
 * file: renca.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#define __USE_BSD
#include <dirent.h>
#include <enca.h>

#ifndef NAME_MAX
# define NAME_MAX 255
#endif
#define NAME_BUFSZ (NAME_MAX + 1)
struct wdlist_t
{
	size_t count;
	struct wdir_t *list;
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
process_name (const char *restrict name, EncaAnalyser *restrict enca, char *restrict buffer)
{
	printf ("PN: %s\n", name);
	return false;
}

bool
process_dir (const char *restrict path, EncaAnalyser *restrict enca)
{
	struct wdlist_t wdlist;
	size_t path_len = 0;
	char *sd_name[2] = {NULL, NULL};
	char namebuff[NAME_BUFSZ];
	DIR *dirp;
	struct dirent *dir_en;
	//printf ("PD: %s\n", path);
	path_len = strlen (path);
	memset (&wdlist, 0, sizeof (struct wdlist_t));
	dirp = opendir (path);
	if (dirp)
	{
		while ((dir_en = readdir (dirp)))
		{
			if (!strcmp (dir_en->d_name, ".") || !strcmp (dir_en->d_name, ".."))
				continue;
			if (process_name (dir_en->d_name, enca, namebuff))
			{
				//strcpy (namebuff, dir_en->d_name); /* debug */
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
				snprintf (sd_name[1], path_len + NAME_BUFSZ, "%s/%s", path, namebuff);
				//printf ("%s -> %s\n", sd_name[0], sd_name[1]); /* debug */
				/* TODO: check exists path */
				if (rename (sd_name[0], sd_name[1]) == -1)
				{
					fprintf (stderr, "FAIL: rename `%s' to `%s'\n", sd_name[0], sd_name[1]);
					perror ("rename");
					continue;
				}
			}
			if (dir_en->d_type == DT_DIR)
				if (!wdir_push (&wdlist, dir_en->d_name))
				{
					fprintf (stderr, "FAIL: %s/%s\n", path, dir_en->d_name);
					perror ("wdir_push");
				}
		}
		closedir (dirp);
		/* TODO: process wdlist here */
		while (wdir_pop (&wdlist, namebuff))
		{
			if (!*sd_name)
			{
				*sd_name = (char*)malloc ((path_len + NAME_BUFSZ));
				if (!*sd_name)
				{
					fprintf (stderr, "FAIL: %s/%s\n", path, namebuff);
					perror ("malloc-wdir_pop");
					continue;
				}
			}
			snprintf (*sd_name, path_len + NAME_BUFSZ, "%s/%s", path, namebuff);
			process_dir (*sd_name, enca);
		}
		if (*sd_name)
			free (*sd_name);
		return true;
	}
	return false;
}

int
main (int argc, char *argv[])
{
	EncaAnalyser enca = NULL;
	if (argc < 2 || !(enca = enca_analyser_alloc (argv[1])))
	{
		fprintf (stderr, "please define two-letter language code in command line\n");
		return EXIT_FAILURE;
	}
	enca_set_threshold (enca, 1.38);
	enca_set_multibyte (enca, 1);
	enca_set_ambiguity (enca, 1);
	enca_set_garbage_test (enca, 1);
	if (!process_dir (".", &enca))
		perror (".");
	enca_analyser_free (enca);
	return EXIT_SUCCESS;
}

