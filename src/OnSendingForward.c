/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int OnSendingForward( struct HetaoEnv *p_env , struct HttpSession *p_http_session )
{
#if ( defined __linux ) || ( defined __unix )
	struct epoll_event	event ;
#elif ( defined _WIN32 )
	struct HttpBuffer	*forward_b = NULL ;
	WSABUF			buf ;
	DWORD			dwFlags ;
#endif
	
	int			nret = 0 ;
	
#if ( defined __linux ) || ( defined __unix )
	/* 发一把HTTP请求 */
	nret = SendHttpRequestNonblock( p_http_session->forward_netaddr.sock , p_http_session->forward_ssl , p_http_session->forward_http ) ;
	if( nret == FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK )
	{
		/* 没发完 */
		DebugLog( __FILE__ , __LINE__ , "SendHttpRequestNonblock return FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK" );
	}
	else if( nret )
	{
		/* 发送报错了 */
		ErrorLog( __FILE__ , __LINE__ , "SendHttpRequestNonblock failed[%d] , errno[%d]" , nret , ERRNO );
		return 1;
	}
#elif ( defined _WIN32 )
	if(
		(
			p_http_session->forward_ssl == NULL
			&&
			GetHttpBufferLengthUnprocessed( GetHttpResponseBuffer(p_http_session->http) ) > 0
		)
		||
		(
			p_http_session->forward_ssl
			&&
			BIO_ctrl_pending( p_http_session->forward_out_bio ) > 0
		)
	)
	{
		/* 继续投递发送事件 */
		if( p_http_session->forward_ssl == NULL )
		{
			forward_b = GetHttpResponseBuffer( p_http_session->forward_http );
			buf.buf = GetHttpBufferBase( forward_b , NULL ) + GetHttpBufferLengthProcessed( forward_b ) ;
			buf.len = GetHttpBufferLengthUnprocessed( forward_b ) ;
		}
		else
		{
			buf.buf = p_http_session->forward_out_bio_buffer ;
			buf.len = BIO_read( p_http_session->forward_out_bio , p_http_session->forward_out_bio_buffer , sizeof(p_http_session->forward_out_bio_buffer)-1 ) ;
		}
		dwFlags = 0 ;
		nret = WSASend( p_http_session->forward_netaddr.sock , & buf , 1 , NULL , dwFlags , & (p_http_session->overlapped) , NULL ) ;
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
	}
#endif
	else
	{
		/* 发送完HTTP请求了 */
		DebugLog( __FILE__ , __LINE__ , "SendHttpRequestNonblock done" );
		
#if ( defined __linux ) || ( defined __unix )
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLIN | EPOLLERR ;
		event.data.ptr = p_http_session ;
		nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->forward_netaddr.sock , & event ) ;
		if( nret == 1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , ERRNO );
			return -1;
		}
#elif ( defined _WIN32 )
		p_http_session->flag = HTTPSESSION_FLAGS_RECEIVING ;
		
		/* 投递接收事件 */
		if( p_http_session->forward_ssl == NULL )
		{
			forward_b = GetHttpResponseBuffer( p_http_session->forward_http );
			buf.buf = GetHttpBufferBase( forward_b , NULL ) ;
			buf.len = GetHttpBufferSize( forward_b ) - 1 ;
		}
		else
		{
			buf.buf = p_http_session->forward_in_bio_buffer ;
			buf.len = sizeof(p_http_session->forward_in_bio_buffer) - 1 ;
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
				SetHttpSessionUnused( p_env , p_http_session );
				return 0;
			}
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "WSARecv ok" );
		}
#endif
	}
	
	return 0;
}
