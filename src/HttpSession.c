/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int IncreaseHttpSessions( struct HetaoEnv *p_env , int http_session_incre_count )
{
	struct HttpSessionArray	*p_http_session_array = NULL ;
	struct HttpSession	*p_http_session = NULL ;
	int			i ;
	
	/* 判断是否到达最大HTTP通讯会话数量 */
	if( p_env->http_session_used_count >= p_env->limits__max_http_session_count )
	{
		WarnLog( __FILE__ , __LINE__ , "http session count limits[%d]" , p_env->limits__max_http_session_count );
		return 1;
	}
	
	/* 批量增加空闲HTTP通讯会话 */
	if( p_env->http_session_used_count + http_session_incre_count > p_env->limits__max_http_session_count )
		http_session_incre_count = p_env->limits__max_http_session_count - p_env->http_session_used_count ;
	
	p_http_session_array = (struct HttpSessionArray *)malloc( sizeof(struct HttpSessionArray) ) ;
	if( p_http_session_array == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , ERRNO );
		return -1;
	}
	memset( p_http_session_array , 0x00 , sizeof(struct HttpSessionArray) );
	list_add_tail( & (p_http_session_array->list) , & (p_env->http_session_array_list.list) );
	
	p_http_session_array->http_session_array = (struct HttpSession *)malloc( sizeof(struct HttpSession) * http_session_incre_count ) ;
	if( p_http_session_array->http_session_array == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , ERRNO );
		return -1;
	}
	memset( p_http_session_array->http_session_array , 0x00 , sizeof(struct HttpSession) * http_session_incre_count );
	
	for( i = 0 , p_http_session = p_http_session_array->http_session_array ; i < http_session_incre_count ; i++ , p_http_session++ )
	{
		p_http_session->http = CreateHttpEnv() ;
		if( p_http_session->http == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "CreateHttpEnv failed , errno[%d]" , ERRNO );
			return -1;
		}
		SetHttpTimeout( p_http_session->http , -1 );
		p_http_session->http_buf = AllocHttpBuffer2( 0 , NULL ) ;
		if( p_http_session->http_buf == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "AllocHttpBuffer2 failed , errno[%d]" , ERRNO );
			return -1;
		}
		p_http_session->forward_http = CreateHttpEnv() ;
		if( p_http_session->forward_http == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "CreateHttpEnv failed , errno[%d]" , ERRNO );
			return -1;
		}
		SetHttpTimeout( p_http_session->forward_http , -1 );
		
		list_add_tail( & (p_http_session->list) , & (p_env->http_session_unused_list.list) );
		DebugLog( __FILE__ , __LINE__ , "init http session[%p] http env[%p]" , p_http_session , p_http_session->http );
	}
	
	p_env->http_session_unused_count += http_session_incre_count ;
	
	return 0;
}

struct HttpSession *FetchHttpSessionUnused( struct HetaoEnv *p_env , unsigned int ip )
{
	struct HttpSession	*p_http_session = NULL ;
	
	int			nret = 0 ;
	
	/* 检查是否达到IP连接数限制 */
	if( p_env->limits__max_connections_per_ip != -1 )
	{
		/* 增加IP连接数计数器 */
		nret = IncreaseIpLimitsHashNode( p_env , ip ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "IncreaseIpLimitsHashNode failed[%d]" , nret );
			return NULL;
		}
	}
	
	if( p_env->http_session_unused_count == 0 )
	{
		/* 如果空闲HTTP通讯会话链表为空 */
		nret = IncreaseHttpSessions( p_env , INCRE_HTTP_SESSION_COUNT ) ;
		if( nret )
			return NULL;
	}
	
	/* 从空闲HTTP通讯会话链表中移出一个会话，并返回之 */
	p_http_session = list_first_entry( & (p_env->http_session_unused_list.list) , struct HttpSession , list ) ;
	list_del( & (p_http_session->list) );
	
	/* 加入到工作HTTP通讯会话树中 */
	p_http_session->timeout_timestamp = (int)GETSECONDSTAMP + p_env->http_options__timeout ;
	nret = AddHttpSessionTimeoutTreeNode( p_env , p_http_session ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "AddTimeoutTreeNode failed , errno[%d]" , ERRNO );
		list_add_tail( & (p_http_session->list) , & (p_env->http_session_unused_list.list) );
		return NULL;
	}
	
	p_http_session->elapse_timestamp = (int)GETSECONDSTAMP + p_env->http_options__elapse ;
	nret = AddHttpSessionElapseTreeNode( p_env , p_http_session ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "AddHttpSessionElapseTreeNode failed , errno[%d]" , ERRNO );
		RemoveHttpSessionTimeoutTreeNode( p_env , p_http_session );
		list_add_tail( & (p_http_session->list) , & (p_env->http_session_unused_list.list) );
		return NULL;
	}
	
	/* 调整计数器 */
	p_env->http_session_used_count++;
	p_env->http_session_unused_count--;
	DebugLog( __FILE__ , __LINE__ , "fetch http session[%p] http env[%p]" , p_http_session , p_http_session->http );
	
	return p_http_session;
}

void SetHttpSessionUnused( struct HetaoEnv *p_env , struct HttpSession *p_http_session )
{
	int		nret = 0 ;
	
	DebugLog( __FILE__ , __LINE__ , "reset http session[%p] http env[%p]" , p_http_session , p_http_session->http );
	
	p_http_session->flag = 0 ;
	
	/* 清理HTTP通讯会话 */
#if ( defined __linux ) || ( defined __unix )
	epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_DEL , p_http_session->netaddr.sock , NULL );
#endif
	if( p_http_session->ssl )
	{
		SSL_shutdown( p_http_session->ssl ) ;
		SSL_free( p_http_session->ssl ) ;
		p_http_session->ssl = NULL ;
		p_http_session->ssl_accepted = 0 ;
	}
	CLOSESOCKET( p_http_session->netaddr.sock );
	ResetHttpEnv( p_http_session->http );
	p_http_session->p_virtual_host = NULL ;
	
	if( p_http_session->p_forward_server )
	{
		SetHttpSessionUnused_05( p_env , p_http_session );
	}
	
	/* 把当前工作HTTP通讯会话移到空闲HTTP通讯会话链表中 */
	if( p_env->limits__max_connections_per_ip != -1 )
	{
		nret = DecreaseIpLimitsHashNode( p_env , (unsigned int)(p_http_session->netaddr.addr.sin_addr.s_addr) ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "DecreaseIpLimitsHashNode failed[%d]" , nret );
		}
	}
	RemoveHttpSessionTimeoutTreeNode( p_env , p_http_session );
	RemoveHttpSessionElapseTreeNode( p_env , p_http_session );
	list_add_tail( & (p_http_session->list) , & (p_env->http_session_unused_list.list) );
	
	p_env->http_session_used_count--;
	p_env->http_session_unused_count++;
	
	return;
}

void SetHttpSessionUnused_05( struct HetaoEnv *p_env , struct HttpSession *p_http_session )
{
	if( p_http_session->forward_ssl )
	{
		SSL_shutdown( p_http_session->forward_ssl ) ;
		SSL_free( p_http_session->forward_ssl ) ;
		p_http_session->forward_ssl = NULL ;
		p_http_session->forward_ssl_connected = 0 ;
	}
	
	if( p_http_session->forward_flags )
	{
		SetHttpSessionUnused_02( p_env , p_http_session );
	}
	
	p_http_session->p_forward_server = NULL ;
	
	return;
}

void SetHttpSessionUnused_02( struct HetaoEnv *p_env , struct HttpSession *p_http_session )
{
#if ( defined __linux ) || ( defined __unix )
	epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_DEL , p_http_session->forward_netaddr.sock , NULL ) ;
#endif
	p_http_session->p_forward_server->connection_count--;
	CLOSESOCKET( p_http_session->forward_netaddr.sock );
	ResetHttpEnv( p_http_session->forward_http );
	p_http_session->forward_flags = 0 ;
	
	return;
}

int ReallocHttpSessionChanged( struct HetaoEnv *p_env , struct HtmlCacheSession *p_htmlcache_session )
{
	struct rb_node		*p_curr = NULL ;
	struct HttpSession	*p_http_session = NULL ;
	struct HttpBuffer	*p_append_buffer = NULL ;
	char			*base = NULL ;
	
	int			nret = 0 ;
	
	p_curr = rb_first( & (p_env->http_session_timeout_rbtree_used) ) ;
	while( p_curr )
	{
		p_http_session = rb_entry( p_curr , struct HttpSession , timeout_rbnode );
		
		p_append_buffer = GetHttpAppendBuffer( p_http_session->http ) ;
		if( p_append_buffer )
		{
			base = GetHttpBufferBase(p_append_buffer,NULL) ;
			
			if( base == p_htmlcache_session->html_gzip_content )
			{
				nret = DuplicateHttpBufferPtr( p_append_buffer ) ;
				if( nret < 0 )
					return -1;
			}
			else if( base == p_htmlcache_session->html_deflate_content )
			{
				nret = DuplicateHttpBufferPtr( p_append_buffer ) ;
				if( nret < 0 )
					return -2;
			}
			else if( base == p_htmlcache_session->html_content )
			{
				nret = DuplicateHttpBufferPtr( p_append_buffer ) ;
				if( nret < 0 )
					return -3;
			}
		}
		
		p_curr = rb_next( p_curr ) ;
	}
	
	return 0;
}
