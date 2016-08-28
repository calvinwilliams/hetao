/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int HtmlCacheEventHander( struct HetaoServer *p_server )
{
	char			inotify_buffer[ 1024 + 1 ] ;
	int			nread ;
	int			npro ;
	struct inotify_event	*p_event = NULL ;
	struct HtmlCacheSession	*p_htmlcache_session = NULL ;
	
	memset( inotify_buffer , 0x00 , sizeof(inotify_buffer) );
	nread = read( p_server->htmlcache_inotify_fd , inotify_buffer , sizeof(inotify_buffer)-1 ) ;
	if( nread <= 0 )
	{
		ErrorLog( __FILE__ , __LINE__ , "read failed[%d] , errno[%d]" , nread , errno );
		return -1;
	}
	else
	{
		DebugLog( __FILE__ , __LINE__ , "read ok , [%d]bytes" , nread );
	}
	
	npro = 0 ;
	while( npro < nread )
	{
		p_event = (struct inotify_event *)(inotify_buffer+npro) ;
		DebugLog( __FILE__ , __LINE__ , "htmlcache inotify event wd[%d] mask[0x%X] len[%d] name[%.*s]" , p_event->wd , p_event->mask , p_event->len , p_event->len , p_event->name );
		
		p_htmlcache_session = QueryHtmlCacheWdTreeNode( p_server , p_event->wd ) ;
		if( p_htmlcache_session )
		{
			/* 发生了监控文件变动通知 */
			
			RemoveHtmlCachePathfilenameTreeNode( p_server , p_htmlcache_session );
			RemoveHtmlCacheWdTreeNode( p_server , p_htmlcache_session );
			inotify_rm_watch( p_server->htmlcache_inotify_fd , p_htmlcache_session->wd );
			DebugLog( __FILE__ , __LINE__ , "inotify_rm_watch[%s] ok , wd[%d]" , p_htmlcache_session->pathfilename , p_htmlcache_session->wd );
			list_del( & (p_htmlcache_session->list) );
			p_server->htmlcache_session_count--;
			FreeHtmlCacheSession( p_htmlcache_session );
		}
		
		npro += sizeof(struct inotify_event) + p_event->len ;
	}
	
	return 0;
}

