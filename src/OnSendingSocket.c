/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int OnSendingSocket( struct HetaoServer *p_server , struct HttpSession *p_http_session )
{
	struct epoll_event	event ;
	
	int			nret = 0 ;
	
	/* 发一把HTTP请求 */
	nret = SendHttpResponseNonblock( p_http_session->netaddr.sock , NULL , p_http_session->http ) ;
	if( nret == FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK )
	{
		/* 没发完 */
		DebugLog( __FILE__ , __LINE__ , "SendHttpResponseNonblock return FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK" );
	}
	else if( nret )
	{
		/* 发送报错了 */
		ErrorLog( __FILE__ , __LINE__ , "SendHttpResponseNonblock failed[%d] , errno[%d]" , nret , errno );
		return 1;
	}
	else
	{
		/* 发送完HTTP响应了 */
		struct HttpEnv		*e = p_http_session->http ;
		
		char			*host = NULL ;
		int			host_len = 0 ;
		char			*user_agent = NULL ;
		int			user_agent_len = 0 ;
		char			*p = NULL ;
		
		char			log_buffer[ 1024 + 1 ] ;
		size_t			log_buflen ;
		
		DebugLog( __FILE__ , __LINE__ , "SendHttpResponseNonblock done" );
		
		if( p_http_session->p_virtualhost )
		{
			/* 输出事件日志 */
			host = QueryHttpHeaderPtr( e , "Host" , & host_len ) ;
			if( host == NULL )
				host = "" ;
			user_agent = QueryHttpHeaderPtr( e , "User-Agent" , & user_agent_len ) ;
			if( user_agent == NULL )
				user_agent = "" ;
			p = strpbrk( user_agent , " \r\n" ) ;
			if( p )
				user_agent_len = p - user_agent ;
			
			log_buffer[sizeof(log_buffer)-1] = '\0' ;
			log_buflen = SNPRINTF( log_buffer , sizeof(log_buffer)-1
				, "%s - %s(%.*s) - %.*s - \"%.*s %.*s %.*s\" %.*s\n"
				, GETDATETIMESTR
				, p_http_session->netaddr.ip
				, user_agent_len , user_agent
				, host_len , host
				, GetHttpHeaderLen_METHOD(e) , GetHttpHeaderPtr_METHOD(e,NULL)
				, GetHttpHeaderLen_URI(e) , GetHttpHeaderPtr_URI(e,NULL)
				, GetHttpHeaderLen_VERSION(e) , GetHttpHeaderPtr_VERSION(e,NULL)
				, GetHttpHeaderLen_STATUSCODE(e) , GetHttpHeaderPtr_STATUSCODE(e,NULL) ) ;
			if( log_buflen != -1 && log_buflen != sizeof(log_buffer)-1 )
				write( p_http_session->p_virtualhost->access_log_fd , log_buffer , log_buflen );
		}
		
		/* 如果有Keep-Alive则不关闭连接，等待下一个HTTP请求 */
		if( CheckHttpKeepAlive(p_http_session->http) )
		{
			DebugLog( __FILE__ , __LINE__ , "Keep-Alive" );
			
			ResetHttpEnv(p_http_session->http);
			
			UpdateHttpSessionTimeoutTreeNode( p_server , p_http_session , GETSECONDSTAMP + p_server->p_config->http_options.timeout );
			
			memset( & event , 0x00 , sizeof(struct epoll_event) );
			event.events = EPOLLIN | EPOLLERR ;
			event.data.ptr = p_http_session ;
			nret = epoll_ctl( p_server->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->netaddr.sock , & event ) ;
			if( nret == 1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , errno );
				return -1;
			}
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "close client socket" );
			return 1;
		}
	}
	
	return 0;
}

