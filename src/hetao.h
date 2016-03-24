/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#ifndef _H_HETAO_
#define _H_HETAO_

#include "fasterhttp.h"

extern char		*__HETAO_VERSION ;

typedef int funcHSProcessHttpRequest( struct HttpUri *p_httpuri , struct HttpEnv *p_httpenv );

#endif

