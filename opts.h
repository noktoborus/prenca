/* vim: ft=c ff=unix fenc=utf-8
 * file: opts.h
 */
#ifndef _OPTS_1362998928_H_
#define _OPTS_1362998928_H_

/* check for not zero-lenght and ';' as eos
 !strcmp (X, ";") */
#define OPTS_CHECK_END(X) (X[0] != '\0')
#define OPTS_CHECK_STOP(X) (X[0] == ';' && X[1] == '\0')

#ifndef MAXPATH
# define MAXPATH 1024
#endif

#define OPTS_HELP_SHORT 1
#define OPTS_HELP_FULL 2

#define OPTS_NOHELP 1

#define OPTS_BADARGS ((void *)-1)
/* void *opts_data, void *cont_data, uint8_t id, size_t argc, char **argv */
typedef void *(*opts_callback_t) (void *, void *, uint8_t, size_t, char **);

struct opts_list_t
{
	char cmd[32];
	uint8_t id;
	uint8_t flags;
	/* */
	opts_callback_t ptr_f;
	size_t argc;
	/* help */
	/* FILE *writeto, char *cmd, uint8_t id, uint8_t type */
	void (*help) (FILE *, char *, uint8_t, uint8_t);
};

void opts_parse (void *opts_data, struct opts_list_t *opts_list, size_t argc, char **argv);
void opts_parse_file (void *opts_data, struct opts_list_t *opts_list, const char *file);

#endif /* _OPTS_1362998928_H_ */

