/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int OnSendingSocket( struct HetaoEnv *p_env , struct HttpSession *p_http_session )
{
#if ( defined __linux ) || ( defined __unix )
	struct epoll_event	event ;
#elif ( defined _WIN32 )
	struct HttpBuffer	*b = NULL ;
	WSABUF			buf ;
	DWORD			dwFlags ;
#endif
	
	int			nret = 0 ;
	
#if ( defined __linux ) || ( defined __unix )
	/* 发一把HTTP响应 */
	nret = SendHttpResponseNonblock( p_http_session->netaddr.sock , p_http_session->ssl , p_http_session->http ) ;
	if( nret == FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK )
	{
		/* 没发完 */
		DebugLog( __FILE__ , __LINE__ , "SendHttpResponseNonblock return FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK" );
		
		UpdateHttpSessionTimeoutTreeNode( p_env , p_http_session , GETSECONDSTAMP + p_env->http_options__timeout );
	}
	else if( nret )
	{
		/* 发送报错了 */
		ErrorLog( __FILE__ , __LINE__ , "SendHttpResponseNonblock failed[%d] , errno[%d]" , nret , ERRNO );
		return 1;
	}
#elif ( defined _WIN32 )
	if(	(
			p_http_session->ssl == NULL
			&&
			( GetHttpBufferLengthUnprocessed( GetHttpResponseBuffer(p_http_session->http) ) > 0 || GetHttpBufferLengthUnprocessed( GetHttpAppendBuffer(p_http_session->http) ) > 0 )
		)
		||
		(
			p_http_session->ssl
			&&
			BIO_ctrl_pending( p_http_session->out_bio ) > 0
		)
	)
	{
		/* 继续投递发送事件 */
		if( p_http_session->ssl == NULL )
		{
			b = GetHttpResponseBuffer( p_http_session->http );
			if( GetHttpBufferLengthUnprocessed(b) == 0 )
			{
				b = GetHttpAppendBuffer(p_http_session->http) ;
			}
			buf.buf = GetHttpBufferBase( b , NULL ) + GetHttpBufferLengthProcessed( b ) ;
			buf.len = GetHttpBufferLengthUnprocessed( b ) ;
		}
		else
		{
			buf.buf = p_http_session->out_bio_buffer ;
			buf.len = BIO_read( p_http_session->out_bio , p_http_session->out_bio_buffer , sizeof(p_http_session->out_bio_buffer)-1 ) ;
		}
		dwFlags = 0 ;
		nret = WSASend( p_http_session->netaddr.sock , & buf , 1 , NULL , dwFlags , & (p_http_session->overlapped) , NULL ) ;
		if( nret == SOCKET_ERROR )
		{
			if( WSAGetLastError() == ERROR_IO_PENDING )
			{
				DebugLog( __FILE__ , __LINE__ , "WSASend io pending" );
			}
			else
			{
				ErrorLog( __FILE__ , __LINE__ , "WSASend failed , errno[%d]" , ERRNO );
				return 1;
			}
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "WSASend ok" );
		}
	}
#endif
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
		
		if( p_http_session->p_virtual_host && p_http_session->p_virtual_host->access_log_fd != -1 )
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
				WRITE( p_http_session->p_virtual_host->access_log_fd , log_buffer , log_buflen );
		}
		
		/* 如果有Keep-Alive则不关闭连接，等待下一个HTTP请求 */
		if( CheckHttpKeepAlive(p_http_session->http) )
		{
			DebugLog( __FILE__ , __LINE__ , "Keep-Alive" );
			
			ResetHttpEnv(p_http_session->http);
			
			UpdateHttpSessionTimeoutTreeNode( p_env , p_http_session , (int)GETSECONDSTAMP + p_env->http_options__timeout );
			
#if ( defined __linux ) || ( defined __unix )
			memset( & event , 0x00 , sizeof(struct epoll_event) );
			event.events = EPOLLIN | EPOLLERR ;
			event.data.ptr = p_http_session ;
			nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->netaddr.sock , & event ) ;
			if( nret == 1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , ERRNO );
				return -1;
			}
#elif ( defined _WIN32 )
			p_http_session->flag = HTTPSESSION_FLAGS_RECEIVING ;
			
			/* 投递接收事件 */
			b = GetHttpRequestBuffer( p_http_session->http );
			if( p_http_session->ssl == NULL )
			{
				buf.buf = GetHttpBufferBase( b , NULL ) ;
				buf.len = GetHttpBufferSize( b ) - 1 ;
			}
			else
			{
				buf.buf = p_http_session->in_bio_buffer ;
				buf.len = sizeof(p_http_session->in_bio_buffer) - 1 ;
			}
			dwFlags = 0 ;
			nret = WSARecv( p_http_session->netaddr.sock , & buf , 1 , NULL , & dwFlags , & (p_http_session->overlapped) , NULL ) ;
			if( nret == SOCKET_ERROR )
			{
				if( WSAGetLastError() == ERROR_IO_PENDING )
				{
					DebugLog( __FILE__ , __LINE__ , "WSARecv io pending" );
				}
				else
				{
					ErrorLog( __FILE__ , __LINE__ , "WSARecv failed , errno[%d]" , ERRNO );
					SetHttpSessionUnused( p_env , p_http_session );
					return 0;
				}
			}
			else
			{
				InfoLog( __FILE__ , __LINE__ , "WSARecv ok" );
			}
#endif
			
			UpdateHttpSessionTimeoutTreeNode( p_env , p_http_session , (int)GETSECONDSTAMP + p_env->http_options__timeout );
			UpdateHttpSessionElapseTreeNode( p_env , p_http_session , (int)GETSECONDSTAMP + p_env->http_options__elapse );
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "close client socket" );
			return 1;
		}
	}
	
	return 0;
}

