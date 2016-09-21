#ifndef __MULTI_DEBUG_H__
#define __MULTI_DEBUG_H__

#define MULTI_DEFAULT_DEBUG_LEVEL 2

#define MULTI_DEBUG  0
#define MULTI_INFO   1
#define MULTI_MSG    2


int multi_debug(int level, const char *fmt, ...);
void multi_debug_init(void);

#endif
