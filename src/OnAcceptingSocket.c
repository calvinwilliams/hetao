/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

#if ( defined __linux ) || ( defined __unix )

int OnAcceptingSocket( struct HetaoEnv *p_env , struct ListenSession *p_listen_session )
{
	int			i ;
	
	SOCKET			sock ;
	struct sockaddr_in	addr ;
	SOCKLEN_T		accept_addr_len ;
	
	struct HttpSession	*p_http_session = NULL ;
	
	struct epoll_event	event ;
	
	int			nret = 0 ;
	
	/* 循环接受所有新连接，直到没有了 */
	for( i = 0 ; i < MAX_LISTEN_EPOLL_EVENTS ; i++ )
	{
		/* 接受新连接 */
		accept_addr_len = sizeof(struct sockaddr) ;
		sock = accept( p_listen_session->netaddr.sock , (struct sockaddr *) & addr , & accept_addr_len );
		if( sock == -1 )
		{
			if( ERRNO == EAGAIN || ERRNO == EWOULDBLOCK )
				break;
			
			ErrorLog( __FILE__ , __LINE__ , "accept failed , errno[%d]" , ERRNO );
			return -1;
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "accept ok , listen #%d# accept #%d#" , p_listen_session->netaddr.sock , sock );
		}
		
		/* 设置TCP选项 */
		SetHttpNonblock( sock );
		SetHttpNodelay( sock , p_env->tcp_options__nodelay );
		SetHttpLinger( sock , p_env->tcp_options__nolinger );
		
		/* 获得一个新的会话结构 */
		p_http_session = FetchHttpSessionUnused( p_env , (unsigned int)(addr.sin_addr.s_addr) ) ;
		if( p_http_session == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "FetchHttpSessionUnused failed , errno[%d]" , ERRNO );
			close( sock );
			return 1;
		}
		p_http_session->type = DATASESSION_TYPE_HTTP ;
		
		p_http_session->p_listen_session = p_listen_session ;
		
		p_http_session->netaddr.sock = sock ;
		memcpy( & (p_http_session->netaddr.addr) , & addr , sizeof(struct sockaddr_in) ) ;
		
		p_http_session->netaddr.ip[sizeof(p_http_session->netaddr.ip)-1] = '\0' ;
		inet_ntop( AF_INET , & (p_http_session->netaddr.addr) , p_http_session->netaddr.ip , sizeof(p_http_session->netaddr.ip) );
		
		/* 注册epoll读事件 */
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLIN | EPOLLERR ;
		event.data.ptr = p_http_session ;
		nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_ADD , p_http_session->netaddr.sock , & event ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "epoll_ctl #%d# add #%d# failed , errno[%d]" , p_env->p_this_process_info->epoll_fd , p_http_session->netaddr.sock , ERRNO );
			SetHttpSessionUnused( p_env , p_http_session );
			return -1;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "epoll_ctl #%d# add #%d#" , p_env->p_this_process_info->epoll_fd , p_http_session->netaddr.sock );
		}
		
		/* SSL握手 */
		if( p_listen_session->ssl_ctx )
		{
			p_http_session->ssl = SSL_new( p_listen_session->ssl_ctx ) ;
			if( p_http_session->ssl == NULL )
			{
				ErrorLog( __FILE__ , __LINE__ , "SSL_new failed , errno[%d]" , ERRNO );
				SetHttpSessionUnused( p_env , p_http_session );
				return 1;
			}
			
			SSL_set_fd( p_http_session->ssl , p_http_session->netaddr.sock );
			
			SSL_set_accept_state( p_http_session->ssl );
			p_http_session->ssl_accepted = 0 ;
			
			nret = OnAcceptingSslSocket( p_env , p_http_session ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "SSL_new failed , errno[%d]" , ERRNO );
				SetHttpSessionUnused( p_env , p_http_session );
				return 0;
			}
			
			return 0;
		}
	}
	
	return 0;
}

#elif ( defined _WIN32 )

int OnAcceptingSocket( struct HetaoEnv *p_env , struct ListenSession *p_listen_session )
{
	struct sockaddr_in	addr ;
	int			addrlen ;
	
	struct HttpSession	*p_http_session = NULL ;
	
	X509			*x509 = NULL ;
	char			*line = NULL ;
	
	struct HttpBuffer	*b = NULL ;
	WSABUF			buf ;
	DWORD			dwFlags ;
	
	int			nret = 0 ;
	HANDLE			hret = NULL ;
	BOOL			bret = TRUE ;
	
	addrlen = sizeof(struct sockaddr) ;
	getpeername( p_listen_session->accept_socket , (struct sockaddr *) & addr , & addrlen );
	
	/* 获得一个新的会话结构 */
	p_http_session = FetchHttpSessionUnused( p_env , (unsigned int)(addr.sin_addr.s_addr) ) ;
	if( p_http_session == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "FetchHttpSessionUnused failed , errno[%d]" , ERRNO );
		CLOSESOCKET( p_http_session->netaddr.sock );
		return 0;
	}
	p_http_session->type = DATASESSION_TYPE_HTTP ;
	p_http_session->flag = HTTPSESSION_FLAGS_RECEIVING ;
	
	p_http_session->p_listen_session = p_listen_session ;
	
	p_http_session->netaddr.sock = p_listen_session->accept_socket ;
	memcpy( & (p_http_session->netaddr.addr) , & addr , sizeof(struct sockaddr_in) ) ;
	
	p_http_session->netaddr.ip[sizeof(p_http_session->netaddr.ip)-1] = '\0' ;
	strncpy( p_http_session->netaddr.ip , inet_ntoa(p_http_session->netaddr.addr.sin_addr) , sizeof(p_http_session->netaddr.ip)-1 );
	
	/* 设置TCP选项 */
	SetHttpNodelay( p_http_session->netaddr.sock , p_env->tcp_options__nodelay );
	SetHttpLinger( p_http_session->netaddr.sock , p_env->tcp_options__nolinger );
	
	/* 绑定完成端口 */
	hret = CreateIoCompletionPort( (HANDLE)(p_http_session->netaddr.sock) , p_env->iocp , (DWORD)p_http_session , 0 ) ;
	if( hret == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "CreateIoCompletionPort failed , errno[%d]" , ERRNO );
		SetHttpSessionUnused( p_env , p_http_session );
		return -1;
	}
	
	/* SSL握手 */
	if( p_listen_session->ssl_ctx )
	{
		p_http_session->ssl = SSL_new( p_listen_session->ssl_ctx ) ;
		if( p_http_session->ssl == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "SSL_new failed , errno[%d]" , ERRNO );
			SetHttpSessionUnused( p_env , p_http_session );
			return 0;
		}
		
		SSL_set_fd( p_http_session->ssl , p_http_session->netaddr.sock );
		
		SSL_set_accept_state( p_http_session->ssl );
		p_http_session->ssl_accepted = 0 ;
		
		/* 设置成非堵塞模式 */
		SetHttpNonblock( p_http_session->netaddr.sock );
		
		nret = OnAcceptingSslSocket( p_env , p_http_session ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "OnAcceptingSslSocket failed , errno[%d]" , ERRNO );
			SetHttpSessionUnused( p_env , p_http_session );
			return 0;
		}
	}
	else
	{
		/* 投递接收事件 */
		b = GetHttpRequestBuffer( p_http_session->http );
		buf.buf = GetHttpBufferBase( b , NULL ) ;
		buf.len = GetHttpBufferSize( b ) - 1 ;
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
	}
	
	/* 继续投递接受新连接事件 */
	p_listen_session->accept_socket = WSASocket( AF_INET , SOCK_STREAM , 0 , NULL , 0 , WSA_FLAG_OVERLAPPED ) ;
	if( p_listen_session->accept_socket == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "WSASocket failed , errno[%d]" , ERRNO );
		return -1;
	}
	
	bret = p_listen_session->lpfnAcceptEx( p_listen_session->netaddr.sock , p_listen_session->accept_socket , p_listen_session->acceptex_buf , 0 , sizeof(struct sockaddr_in)+16 , sizeof(struct sockaddr_in)+16 , NULL , & (p_listen_session->overlapped) ) ;
	if( bret != TRUE )
	{
		if( WSAGetLastError() == ERROR_IO_PENDING )
		{
			DebugLog( __FILE__ , __LINE__ , "AcceptEx io pending" );
		}
		else
		{
			ErrorLog( __FILE__ , __LINE__ , "AcceptEx failed , errno[%d]" , ERRNO );
			return -1;
		}
	}
	
	return 0;
}

#endif

