/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

void FreeHtmlCacheSession( struct HtmlCacheSession *p_htmlcache_session )
{
	if( p_htmlcache_session->pathfilename )
		free( p_htmlcache_session->pathfilename );
	if( p_htmlcache_session->html_content )
		free( p_htmlcache_session->html_content );
	if( p_htmlcache_session->html_gzip_content )
		free( p_htmlcache_session->html_gzip_content );
	if( p_htmlcache_session->html_deflate_content )
		free( p_htmlcache_session->html_deflate_content );
	free( p_htmlcache_session );
	return;
}

