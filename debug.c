#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include "debug.h"
#include "parse_options.h"

static pthread_mutex_t debug_lock;

void multi_debug_init(void)
{
	pthread_mutex_init(&debug_lock, 0);
}

int multi_debug(int level, const char *fmt, ...)
{
	if (level<multi.loglevel) return 0;
	if (!multi.debug) return 0;


	char tstr[64];
	time_t t=time(0);
	struct tm *lt;
	lt=localtime(&t);


	strftime(tstr, 64, "%Y-%m-%d %H:%M:%S", lt);

	pthread_mutex_lock(&debug_lock);
	fprintf(multi.debug, "multifs [%s]", tstr);

	switch(level)
	{
		case MULTI_DEBUG: fprintf(multi.debug, " (debug): "); break;
		case MULTI_INFO:  fprintf(multi.debug, " (info): ");  break;
		default:         fprintf(multi.debug, ": ");  break;
	}

	fprintf(multi.debug, "[%ld] ", pthread_self());

	va_list ap;
	va_start(ap, fmt);
	int res=vfprintf(multi.debug, fmt, ap);
	va_end(ap);
	pthread_mutex_unlock(&debug_lock);
	return res;
}

