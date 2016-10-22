/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

#if ( defined __linux ) || ( defined __unix )

int HtmlCacheEventHander( struct HetaoEnv *p_env )
{
	char			inotify_buffer[ 1024 + 1 ] ;
	int			nread ;
	int			npro ;
	struct inotify_event	*p_event = NULL ;
	struct HtmlCacheSession	*p_htmlcache_session = NULL ;
	
	int			nret = 0 ;
	
	memset( inotify_buffer , 0x00 , sizeof(inotify_buffer) );
	nread = read( p_env->htmlcache_inotify_fd , inotify_buffer , sizeof(inotify_buffer)-1 ) ;
	if( nread <= 0 )
	{
		ErrorLog( __FILE__ , __LINE__ , "read failed[%d] , errno[%d]" , nread , ERRNO );
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
		InfoLog( __FILE__ , __LINE__ , "htmlcache inotify event wd[%d] mask[0x%X] len[%d] name[%.*s]" , p_event->wd , p_event->mask , p_event->len , p_event->len , p_event->name );
		
		p_htmlcache_session = QueryHtmlCacheWdTreeNode( p_env , p_event->wd ) ;
		if( p_htmlcache_session )
		{
			/* 发生了监控文件变动通知 */
			nret = ReallocHttpSessionChanged( p_env , p_htmlcache_session ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "ReallocHttpSessionChanged failed[%d] , errno[%d]" , nret , ERRNO );
				return nret;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "ReallocHttpSessionChanged ok" );
			}
			
			RemoveHtmlCachePathfilenameTreeNode( p_env , p_htmlcache_session );
			RemoveHtmlCacheWdTreeNode( p_env , p_htmlcache_session );
			inotify_rm_watch( p_env->htmlcache_inotify_fd , p_htmlcache_session->wd );
			DebugLog( __FILE__ , __LINE__ , "inotify_rm_watch[%s] ok , wd[%d]" , p_htmlcache_session->pathfilename , p_htmlcache_session->wd );
			list_del( & (p_htmlcache_session->list) );
			p_env->htmlcache_session_count--;
			FreeHtmlCacheSession( p_htmlcache_session , 1 );
		}
		
		npro += sizeof(struct inotify_event) + p_event->len ;
	}
	
	return 0;
}

#elif ( defined _WIN32 )

int DirectoryWatcherEventHander( struct HetaoEnv *p_env , struct VirtualHost *p_virtual_host )
{
	FILE_NOTIFY_INFORMATION	*p_notify = NULL ;
	char			filename[ MAX_PATH + 1 ] ;
	char			pathfilename[ MAX_PATH + 1 ] ;
	struct HtmlCacheSession	*p_htmlcache_session = NULL ;
	
	int			nret = 0 ;
	
	p_notify = (FILE_NOTIFY_INFORMATION *)(p_virtual_host->directory_changes_buffer) ;
	while( p_notify )
	{
		memset( filename , 0x00 , sizeof(filename) );
		WideCharToMultiByte( CP_ACP , 0 , p_notify->FileName , p_notify->FileNameLength/2 , filename , sizeof(filename)-1 , NULL , NULL );
		
		InfoLog( __FILE__ , __LINE__ , "directory changes event event action[%ld] filename[%s]" , p_notify->Action , filename );
		
		memset( pathfilename , 0x00 , sizeof(pathfilename) );
		SNPRINTF( pathfilename , sizeof(pathfilename)-1 , "%s/%s" , p_virtual_host->wwwroot , filename );
		
		p_htmlcache_session = QueryHtmlCachePathfilenameTreeNode( p_env , pathfilename ) ;
		if( p_htmlcache_session )
		{
			/* 发生了监控文件变动通知 */
			nret = ReallocHttpSessionChanged( p_env , p_htmlcache_session ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "ReallocHttpSessionChanged failed[%d] , errno[%d]" , nret , ERRNO );
				return nret;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "ReallocHttpSessionChanged ok" );
			}
			
			RemoveHtmlCachePathfilenameTreeNode( p_env , p_htmlcache_session );
			RemoveHtmlCacheWdTreeNode( p_env , p_htmlcache_session );
			DebugLog( __FILE__ , __LINE__ , "RemoveHtmlCachePathfilenameTreeNode ok , pathfilename[%s]" , pathfilename );
			list_del( & (p_htmlcache_session->list) );
			p_env->htmlcache_session_count--;
			FreeHtmlCacheSession( p_htmlcache_session , 1 );
		}
		
		if( p_notify->NextEntryOffset == 0 )
			break;
		p_notify = (FILE_NOTIFY_INFORMATION *)( (char*)p_notify + p_notify->NextEntryOffset ) ;
	}
	
	return 0;
}

#endif
