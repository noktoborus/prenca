/* vim: ft=c ff=unix fenc=utf-8
 * file: opts.c
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "opts.h"

static size_t
_opts_parse_args (int argc, char **argv, size_t needle)
{
	register size_t no = 0;
	/* check for stop symbols (';') */
	if (argc < needle)
	{
		return (size_t)-1;
	}
	for (no = 0; no < needle && !(OPTS_CHECK_END (argv[no]) && OPTS_CHECK_STOP (argv[no])); no++);
	/* check end of args or ';' */
	if (no == needle && (no == argc || (OPTS_CHECK_END (argv[no]) && OPTS_CHECK_STOP (argv[no]))))
	{
		if (no != argc && OPTS_CHECK_STOP (argv[no]))
			no++;
		return no;
	}
	return (size_t)-1;
}

void
opts_parse (void *opts_data, struct opts_list_t *opts_list, size_t argc, char **argv)
{
	register struct opts_list_t *opttest;
	register size_t counter = 0;
	register size_t no = 0;
	register size_t len;
	register size_t args_cc = 0;
	void *result = NULL;
	/* print help */
	if (!argc || !strncmp (argv[0], "?", 2))
	{
		opttest = opts_list;
		if (argc > 1)
		{
			len = strlen (argv[1]);
			/* print help for commands */
			for (; opttest->cmd[0]; opttest++)
			{
				if (!strncmp (argv[1], opttest->cmd, len) && (!(opttest->flags & OPTS_NOHELP) && opttest->help))
				{
					fprintf (stderr, "* %s\n", opttest->cmd);
					opttest->help (stderr, opttest->cmd, opttest->id, OPTS_HELP_SHORT | OPTS_HELP_FULL);
				}
			}
		}
		else
		{
			/* print help for all list */
			for (; opttest->cmd[0]; opttest++)
			{
				if (!(opttest->flags & OPTS_NOHELP) && opttest->help)
					opttest->help (stderr, opttest->cmd, opttest->id, OPTS_HELP_SHORT);
			}
		}
		return;
	}
	/* process args */
	for (; no < argc && !counter; no++)
	{
		opttest = opts_list;
		len = strlen (argv[no]);
		while (opttest->cmd[0])
		{
			if (!strncmp (argv[no], opttest->cmd, len + 1)\
				&& ((args_cc = _opts_parse_args (argc - no - 1, argv + no + 1, opttest->argc)) != (size_t)-1))
			{
				result = NULL;
				while ((result = opttest->ptr_f (opts_data, result, opttest->id, opttest->argc, argv + no + 1)))
				{
					/* skip 1 + args_cc */
					register size_t i = no + args_cc + 1;
					counter++;
					/* print help and exit */
					if (result == OPTS_BADARGS)
					{
						if (opttest->help)
							opttest->help (stderr, opttest->cmd, opttest->id, OPTS_HELP_SHORT);
						break;
					}
					/* parse repeat, if func return data */
					if (argc - i)
						opts_parse (opts_data, opts_list, argc - i, argv + i);
				}
				/* skip parsed (and final ';') symbos */
				no += args_cc;
				break;
			}
			opttest++;
		}
		if (!opttest->cmd[0])
		{
			fprintf (stderr, "unknown key: '%s'\n", argv[no]);
		}
	}
}

static void
_opts_parse_file_line (void *opts_data, struct opts_list_t *opts_list, char *line)
{
	char **argv;
	size_t no = 0;
	size_t len = strlen (line);
	size_t argc = 0;
	bool quoted = false;
	uint8_t escaped = 0;
	/* count spaces */
	for (argc = 1, no = 0; no < len; no++)
		if (isblank (line[no])) argc++;
	/* alloc argv */
	argv = (char **)calloc ((argc ), sizeof (char **));
	if (!argv)
		return;
	for (argc = 1, argv[0] = line, no = 0; no < len; no ++)
	{
		/* if (isblank (line[no]))
		* printf ("BL: 0x%x (%s), no: %d\n", line[no], argv[argc], no);
		*/
		if (line[no] == '\\' && !escaped)
		{
			escaped = 2;
			memmove ((void *)&line[no], &line[no + 1], len - no);
			no--;
			len--;
		}
		else
		if (line[no] == '"' && !escaped)
		{
			quoted = !quoted;
			memmove ((void *)&line[no], &line[no + 1], len - no);
			no--;
			len--;
		}
		else
		if (isblank (line[no]) && !quoted && !escaped)
		{
			line[no] = '\0';
			if (no > 0 && line[no - 1])
				argv[argc++] = line + no + 1;
			else
				argv[argc - 1] = line + no + 1;
		}
		if (escaped)
			escaped --;
	}
	opts_parse (opts_data, opts_list, argc, argv);
	free (argv);
}

void
opts_parse_file (void *opts_data, struct opts_list_t *opts_list, const char *file)
{
	/* 1. alloc buffer: filesize +1
	*  2. read all
	*  3. allow array (count elements: all spaces and new lines in file)
	*  4. split to array
	*  5. pass to opts_parse ()
	*/
	struct stat stat;
	int fd;
	char *buf;
	char *line_start;
	size_t no;
	if ((fd = open (file, O_RDONLY)) == -1)
		return;
	if (!fstat (fd, &stat))
	{
		if (stat.st_size > 0 && (buf = calloc (1, stat.st_size + 1)))
		{
			if (read (fd, buf, stat.st_size) == stat.st_size)
			{
				for (no = 0, line_start = buf; no <= stat.st_size; no++)
				{
					if (no == stat.st_size || (buf[no] == '\n' || buf[no] == '\r'))
					{
						buf[no] = '\0';
						/* execute if line not zero-length */
						if (line_start[0] != '\0')
							_opts_parse_file_line (opts_data, opts_list, line_start);
						line_start = buf + no + 1;
					}
				}
			}
			free (buf);
		}
	}
	close (fd);
}

