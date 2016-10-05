/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int OnReceivingForward( struct HetaoEnv *p_env , struct HttpSession *p_http_session )
{
#if ( defined _WIN32 )
	struct HttpBuffer	*forward_b = NULL ;
#endif
	struct HttpBuffer	*b = NULL ;
	char			*response_base = NULL ;
	int			response_len ;
	
	int			nret = 0 ;
	
#if ( defined __linux ) || ( defined __unix )
	struct epoll_event	event ;
	
	/* 收一把HTTP响应 */
	nret = ReceiveHttpResponseNonblock( p_http_session->forward_netaddr.sock , p_http_session->forward_ssl , p_http_session->forward_http ) ;
	if( nret == FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER )
	{
		/* 没收完整 */
		DebugLog( __FILE__ , __LINE__ , "ReceiveHttpResponseNonblock return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER" );
	}
	else if( nret )
	{
		/* 接收报错了 */
		if( nret == FASTERHTTP_ERROR_TCP_CLOSE )
		{
			ErrorLog( __FILE__ , __LINE__ , "http socket closed detected" );
			return 1;
		}
		else if( nret == FASTERHTTP_INFO_TCP_CLOSE )
		{
			InfoLog( __FILE__ , __LINE__ , "http socket closed detected" );
			return 1;
		}
		else
		{
			ErrorLog( __FILE__ , __LINE__ , "ReceiveHttpResponseNonblock failed[%d] , errno[%d]" , nret , ERRNO );
			
			nret = FormatHttpResponseStartLine( abs(nret)/100 , p_http_session->http , 1 ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "FormatHttpResponseStartLine failed[%d] , errno[%d]" , nret , ERRNO );
				return 1;
			}
			
			return 0;
		}
	}
#elif ( defined _WIN32 )
	WSABUF			buf ;
	DWORD			dwFlags ;
	
	/* 解析一把HTTP请求 */
	nret = ParseHttpResponse( p_http_session->forward_http ) ;
	if( nret == FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER )
	{
		/* 没收完整 */
		DebugLog( __FILE__ , __LINE__ , "ParseHttpRequest return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER" );
		
		UpdateHttpSessionTimeoutTreeNode( p_env , p_http_session , (int)GETSECONDSTAMP + p_env->http_options__timeout );
		
		/* 继续投递接收事件 */
		if( p_http_session->forward_ssl == NULL )
		{
			forward_b = GetHttpResponseBuffer( p_http_session->forward_http );
			buf.buf = GetHttpBufferBase( forward_b , NULL ) + GetHttpBufferLength( forward_b ) ;
			buf.len = GetHttpBufferSize( forward_b ) - 1 - GetHttpBufferLength( forward_b ) ;
		}
		else
		{
			buf.buf = p_http_session->forward_out_bio_buffer ;
			buf.len = BIO_read( p_http_session->forward_out_bio , p_http_session->forward_out_bio_buffer , sizeof(p_http_session->forward_out_bio_buffer)-1 ) ;
		}
		dwFlags = 0 ;
		nret = WSARecv( p_http_session->forward_netaddr.sock , & buf , 1 , NULL , & dwFlags , & (p_http_session->overlapped) , NULL ) ;
		if( nret == SOCKET_ERROR )
		{
			if( WSAGetLastError() == ERROR_IO_PENDING )
			{
				DebugLog( __FILE__ , __LINE__ , "WSARecv io pending" );
			}
			else
			{
				ErrorLog( __FILE__ , __LINE__ , "WSARecv failed , errno[%d]" , ERRNO );
				return 1;
			}
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "WSARecv ok" );
		}
	}
	else if( nret )
	{
		/* 接收报错了 */
		if( nret == FASTERHTTP_ERROR_TCP_CLOSE )
		{
			ErrorLog( __FILE__ , __LINE__ , "http socket closed detected" );
			return 1;
		}
		else if( nret == FASTERHTTP_INFO_TCP_CLOSE )
		{
			InfoLog( __FILE__ , __LINE__ , "http socket closed detected" );
			return 1;
		}
		else
		{
			ErrorLog( __FILE__ , __LINE__ , "ParseHttpRequest failed[%d] , errno[%d]" , nret , ERRNO );
			return 1;
		}
	}
#endif
	else
	{
		/* 收满一个HTTP响应 */
		DebugLog( __FILE__ , __LINE__ , "ReceiveHttpResponseNonblock done" );
		p_http_session->flag = HTTPSESSION_FLAGS_SENDING ;
		
		/* 复制HTTP响应 */
		response_base = GetHttpBufferBase( GetHttpResponseBuffer(p_http_session->forward_http) , & response_len ) ;
		b = GetHttpResponseBuffer(p_http_session->http) ;
		nret = MemcatHttpBuffer( b , response_base , response_len ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "MemcatHttpBuffer failed , errno[%d]" , ERRNO );
			return 1;
		}
		
		DebugHexLog( __FILE__ , __LINE__ , GetHttpBufferBase(b,NULL) , GetHttpBufferLength(b) , "HttpResponseBuffer [%d]bytes" , GetHttpBufferLength(b) );
		
		SetHttpSessionUnused_02( p_env , p_http_session );
		
#if ( defined __linux ) || ( defined __unix )
		/* 恢复原连接事件 */
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLOUT | EPOLLERR ;
		event.data.ptr = p_http_session ;
		nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->netaddr.sock , & event ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , ERRNO );
			return -1;
		}
#elif ( defined _WIN32 )
		/* 继续投递发送事件 */
		if( p_http_session->ssl == NULL )
		{
			b = GetHttpResponseBuffer( p_http_session->http );
			buf.buf = GetHttpBufferBase( b , NULL ) + GetHttpBufferLengthProcessed( b ) ;
			buf.len = GetHttpBufferLengthUnprocessed( b ) ;
		}
		else
		{
			b = GetHttpResponseBuffer( p_http_session->http );
			SSL_write( p_http_session->ssl , GetHttpBufferBase( b , NULL ) , GetHttpBufferLength( b ) );
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
#endif
	}
	
	return 0;
}
