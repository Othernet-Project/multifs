/*
   multifs - Multi HDD [FUSE] File System
   Copyright (C) 2016 Outernet Inc <abhishek@outernet.is>

   derived from mhddfs
   Copyright (C) 2008 Dmitry E. Oboukhov <dimka@avanto.org>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <stddef.h>
#include <fuse.h>

#include "parse_options.h"
#include "usage.h"
#include "version.h"
#include "debug.h"
#include "tools.h"

struct multi_config multi={0};

#define MULTIFS_OPT(t, p, v) { t, offsetof(struct multi_config, p), v }
#define MULTI_VERSION_OPT 15121974


#if FUSE_VERSION >= 27
#define FUSE_MP_OPT_STR "-osubtype=multifs,fsname="
#else
#define FUSE_MP_OPT_STR "-ofsname=multifs#"
#endif

/* the number less (or equal) than 100 is in percent,
   more than 100 is in bytes */
#define DEFAULT_MLIMIT ( 10 * 1024 * 1024 )
#define MINIMUM_MLIMIT ( 1 * 1024 * 1024 )

static struct fuse_opt multifs_opts[]={
	MULTIFS_OPT("mlimit=%s",   mlimit_str, 0),
	MULTIFS_OPT("logfile=%s",  debug_file, 0),
	MULTIFS_OPT("loglevel=%d", loglevel,   0),

	FUSE_OPT_KEY("-V",        MULTI_VERSION_OPT),
	FUSE_OPT_KEY("--version", MULTI_VERSION_OPT),

	FUSE_OPT_END
};

static void add_multi_dirs(const char * dir)
{
	int i;
	char ** newdirs;
	char *add_dir;


	if (*dir=='/')
	{
		add_dir=strdup(dir);
	}
	else
	{
		char cpwd[PATH_MAX];
		getcwd(cpwd, PATH_MAX);
		add_dir = create_path(cpwd, dir);
	}

	if (!multi.dirs)
	{
		multi.dirs=calloc(2, sizeof(char *));
		multi.dirs[0]=add_dir;
		multi.cdirs=1;
		return;
	}

	newdirs=calloc(multi.cdirs+2, sizeof(char *));
	for (i=0; i<multi.cdirs; i++)
	{
		newdirs[i]=multi.dirs[i];
	}
	newdirs[multi.cdirs++]=add_dir;
	free(multi.dirs);
	multi.dirs=newdirs;
}

static int multifs_opt_proc(void *data,
		const char *arg, int key, struct fuse_args *outargs)
{
	switch(key)
	{
		case MULTI_VERSION_OPT:
			fprintf(stderr, "multifs version: %s\n", VERSION);
			exit(0);

		case FUSE_OPT_KEY_NONOPT:
			{
				char *dir = strdup(arg);
				char *tmp;
				for (tmp=dir; tmp; tmp=strchr(tmp+1, ','))
				{
					if (*tmp==',') tmp++;
					char *add=strdup(tmp);
					char *end=strchr(add, ',');
					if (end) *end=0;
					add_multi_dirs(add);
					free(add);
				}
				free(dir);
				return 0;
			}
	}
	return 1;
}

static void check_if_unique_mountpoints(void)
{
	int i, j;
	struct stat * stats = calloc(multi.cdirs, sizeof(struct stat));

	for (i = 0; i < multi.cdirs; i++) {
		if (stat(multi.dirs[i], stats + i) != 0)
			memset(stats + i, 0, sizeof(struct stat));


		for (j = 0; j < i; j++) {
			if (strcmp(multi.dirs[i], multi.dirs[j]) != 0) {
				/*  mountdir isn't unique */
				if (stats[j].st_dev != stats[i].st_dev)
					continue;
				if (stats[j].st_ino != stats[i].st_ino)
					continue;
				if (!stats[i].st_dev)
					continue;
				if (!stats[i].st_ino)
					continue;
			}

			fprintf(stderr,
				"multifs: Duplicate directories: %s %s\n"
				"\t%s was excluded from dirlist\n",
				multi.dirs[i],
				multi.dirs[j],
				multi.dirs[i]
			);

			free(multi.dirs[i]);
			multi.dirs[i] = 0;

			for (j = i; j < multi.cdirs - 1; j++)
				multi.dirs[j] = multi.dirs[j+1];
			multi.cdirs--;
			i--;
			break;
		}
	}

	free(stats);
}

struct fuse_args * parse_options(int argc, char *argv[])
{
	struct fuse_args * args=calloc(1, sizeof(struct fuse_args));
	char * info;
	int i,  l;

	{
		struct fuse_args tmp=FUSE_ARGS_INIT(argc, argv);
		memcpy(args, &tmp, sizeof(struct fuse_args));
	}

	multi.loglevel=MULTI_DEFAULT_DEBUG_LEVEL;
	if (fuse_opt_parse(args, &multi, multifs_opts, multifs_opt_proc)==-1)
		usage(stderr);

	if (multi.cdirs<3) usage(stderr);
	multi.mount=multi.dirs[--multi.cdirs];
	multi.dirs[multi.cdirs]=0;

	check_if_unique_mountpoints();

	for (i=l=0; i<multi.cdirs; i++) l += strlen(multi.dirs[i])+2;
	l += sizeof(FUSE_MP_OPT_STR);
	info = calloc(l, sizeof(char));
	strcat(info, FUSE_MP_OPT_STR);
	for (i=0; i<multi.cdirs; i++)
	{
		if (i) strcat(info, ";");
		strcat(info, multi.dirs[i]);
	}
	fuse_opt_insert_arg(args, 1, info);
	fuse_opt_insert_arg(args, 1, multi.mount);
	free(info);

	if (multi.cdirs)
	{
		int i;
		for(i=0; i<multi.cdirs; i++)
		{
			struct stat info;
			if (stat(multi.dirs[i], &info))
			{
				fprintf(stderr,
					"multifs: can not stat '%s': %s\n",
					multi.dirs[i], strerror(errno));
				exit(-1);
			}
			if (!S_ISDIR(info.st_mode))
			{
				fprintf(stderr,
					"multifs: '%s' - is not directory\n\n",
					multi.dirs[i]);
				exit(-1);
			}

			fprintf(stderr,
				"multifs: directory '%s' added to list\n",
				multi.dirs[i]);
		}
	}

	fprintf(stderr, "multifs: mount to: %s\n", multi.mount);

	if (multi.debug_file)
	{
		fprintf(stderr, "multifs: using debug file: %s, loglevel=%d\n",
				multi.debug_file, multi.loglevel);
		multi.debug=fopen(multi.debug_file, "a");
		if (!multi.debug)
		{
			fprintf(stderr, "Can not open file '%s': %s",
					multi.debug_file,
					strerror(errno));
			exit(-1);
		}
		setvbuf(multi.debug, NULL, _IONBF, 0);
	}

	multi.move_limit = DEFAULT_MLIMIT;

	if (multi.mlimit_str)
	{
		int len = strlen(multi.mlimit_str);

		if (len) {
			switch(multi.mlimit_str[len-1])
			{
				case 'm':
				case 'M':
					multi.mlimit_str[len-1]=0;
					multi.move_limit=atoll(multi.mlimit_str);
					multi.move_limit*=1024*1024;
					break;
				case 'g':
				case 'G':
					multi.mlimit_str[len-1]=0;
					multi.move_limit=atoll(multi.mlimit_str);
					multi.move_limit*=1024*1024*1024;
					break;

				case 'k':
				case 'K':
					multi.mlimit_str[len-1]=0;
					multi.move_limit=atoll(multi.mlimit_str);
					multi.move_limit*=1024;
					break;

				case '%':
					multi.mlimit_str[len-1]=0;
					multi.move_limit=atoll(multi.mlimit_str);
					break;

				default:
					multi.move_limit=atoll(multi.mlimit_str);
					break;
			}
		}

		if (multi.move_limit < MINIMUM_MLIMIT) {
			if (!multi.move_limit) {
				multi.move_limit = DEFAULT_MLIMIT;
			} else {
				if (multi.move_limit > 100)
					multi.move_limit = MINIMUM_MLIMIT;
			}
		}
	}
	/*
	if (multi.move_limit <= 100)
		fprintf(stderr, "multifs: move size limit %lld%%\n",
				(long long)multi.move_limit);
	else
		fprintf(stderr, "multifs: move size limit %lld bytes\n",
				(long long)multi.move_limit);
	*/

	multi_debug(MULTI_MSG, " >>>>> multi " VERSION " started <<<<<\n");

	return args;
}
