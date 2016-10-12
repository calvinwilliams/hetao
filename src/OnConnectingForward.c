/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int SelectForwardAddress( struct HetaoEnv *p_env , struct HttpSession *p_http_session )
{
	/* 轮询算法 */
	if( p_http_session->p_virtualhost->forward_rule[0] == FORWARD_RULE_ROUNDROBIN[0] )
	{
		struct list_head	*p_curr = NULL , *p_next = NULL ;
		struct ForwardServer	*p_forward_server = NULL ;
		
		list_for_each_safe( p_curr , p_next , & (p_http_session->p_virtualhost->roundrobin_list.roundrobin_node) )
		{
			p_forward_server = container_of( p_curr , struct ForwardServer , roundrobin_node ) ;
			
			if( p_forward_server->timestamp_to_valid == 0 || GETSECONDSTAMP > p_forward_server->timestamp_to_valid )
			{
				p_forward_server->timestamp_to_valid = 0 ;
				
				list_move_tail( p_curr , & (p_http_session->p_virtualhost->roundrobin_list.roundrobin_node) );
				p_http_session->p_forward_server = p_forward_server ;
				DebugLog( __FILE__ , __LINE__ , "forward_rule[B] server[%s:%d]" , p_http_session->p_forward_server->netaddr.ip , p_http_session->p_forward_server->netaddr.port );
				
				return HTTP_OK;
			}
		}
		
		return HTTP_SERVICE_UNAVAILABLE;
	}
	/* 最少连接数算法 */
	else if( p_http_session->p_virtualhost->forward_rule[0] == FORWARD_RULE_LEASTCONNECTION[0] )
	{
		struct ForwardServer	*p_forward_server = NULL ;
		
		p_forward_server = TravelMinLeastConnectionCountTreeNode( p_http_session->p_virtualhost , NULL ) ;
		while(p_forward_server)
		{
			if( p_forward_server->timestamp_to_valid == 0 || GETSECONDSTAMP > p_forward_server->timestamp_to_valid )
			{
				p_forward_server->timestamp_to_valid = 0 ;
				
				p_http_session->p_forward_server = p_forward_server ;
				DebugLog( __FILE__ , __LINE__ , "forward_rule[L] server[%s:%d]" , p_http_session->p_forward_server->netaddr.ip , p_http_session->p_forward_server->netaddr.port );
				
				return HTTP_OK;
			}
			
			p_forward_server = TravelMinLeastConnectionCountTreeNode( p_http_session->p_virtualhost , p_forward_server ) ;
		}
		
		return HTTP_SERVICE_UNAVAILABLE;
	}
	else
	{
		ErrorLog( __FILE__ , __LINE__ , "forward_rule[%c] invalid" , p_http_session->p_virtualhost->forward_rule[0] );
		return HTTP_INTERNAL_SERVER_ERROR;
	}
}

int ConnectForwardServer( struct HetaoEnv *p_env , struct HttpSession *p_http_session )
{
#if ( defined __linux ) || ( defined __unix )
	struct epoll_event	event ;
#elif ( defined _WIN32 )
	struct HttpBuffer	*b = NULL ;
	WSABUF			buf ;
	DWORD			dwBytesSent ;
	DWORD			dwFlags ;
	HANDLE			hret ;
	BOOL			bret ;
#endif
	
	char			*request_base = NULL ;
	int			request_len ;
	struct HttpBuffer	*forward_b = NULL ;
	
	int			nret = 0 ;
	
	p_http_session->forward_flags = 0 ;
	
	memset( & (p_http_session->forward_netaddr) , 0x00 , sizeof(struct NetAddress) );
	
	/* 创建转发连接 */
#if ( defined __linux ) || ( defined __unix )
	p_http_session->forward_netaddr.sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
	if( p_http_session->forward_netaddr.sock == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "socket failed , errno[%d]" , ERRNO );
		return HTTP_INTERNAL_SERVER_ERROR;
	}
#elif ( defined _WIN32 )
	p_http_session->forward_netaddr.sock = WSASocket( AF_INET , SOCK_STREAM , 0 , NULL , 0 , WSA_FLAG_OVERLAPPED ) ;
	if( p_http_session->forward_netaddr.sock == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "WSASocket failed , errno[%d]" , ERRNO );
		return HTTP_INTERNAL_SERVER_ERROR;
	}
#endif
	
#if ( defined __linux ) || ( defined __unix )
	SetHttpNonblock( p_http_session->forward_netaddr.sock );
#endif
	SetHttpNodelay( p_http_session->forward_netaddr.sock , p_env->tcp_options__nodelay );
	SetHttpLinger( p_http_session->forward_netaddr.sock , p_env->tcp_options__nolinger );
	
#if ( defined _WIN32 )
	/* 绑定完成端口  */
	hret = CreateIoCompletionPort( (HANDLE)(p_http_session->forward_netaddr.sock) , p_env->iocp , (DWORD)p_http_session , 0 ) ;
	if( hret == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "CreateIoCompletionPort failed , errno[%d]" , ERRNO );
		return HTTP_INTERNAL_SERVER_ERROR;
	}
#endif
	
#if ( defined _WIN32 )
	/* 绑定端口，完成端口特殊需要 */
	SETNETADDRESS( p_http_session->forward_netaddr )
	nret = bind( p_http_session->forward_netaddr.sock , (struct sockaddr *) & (p_http_session->forward_netaddr.addr) , sizeof(struct sockaddr) ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "bind[%s:%d] failed , errno[%d]" , p_http_session->forward_netaddr.ip , p_http_session->forward_netaddr.port , ERRNO );
		return HTTP_INTERNAL_SERVER_ERROR;
	}
#endif
	
	p_http_session->forward_flags = HTTPSESSION_FLAGS_CONNECTING ;
	p_http_session->p_forward_server->connection_count++;
	UpdateLeastConnectionCountTreeNode( p_http_session->p_virtualhost , p_http_session->p_forward_server );
	
	/* 连接服务端 */
	DebugLog( __FILE__ , __LINE__ , "connecting[%s:%d] ..." , p_http_session->p_forward_server->netaddr.ip , p_http_session->p_forward_server->netaddr.port );
#if ( defined __linux ) || ( defined __unix )
	nret = connect( p_http_session->forward_netaddr.sock , (struct sockaddr *) & (p_http_session->p_forward_server->netaddr.addr) , sizeof(struct sockaddr) ) ;
	if( nret == -1 )
#elif ( defined _WIN32 )
	dwBytesSent = 0 ;
	memset( & (p_http_session->overlapped) , 0x00 , sizeof(p_http_session->overlapped) );
	bret = p_env->lpfnConnectEx( p_http_session->forward_netaddr.sock , (struct sockaddr *) & (p_http_session->p_forward_server->netaddr.addr) , sizeof(struct sockaddr) , NULL , 0 , & dwBytesSent , & (p_http_session->overlapped) ) ;
	if( bret == FALSE )
#endif
	{
#if ( defined __linux ) || ( defined __unix )
		if( ERRNO == EINPROGRESS )
#elif ( defined _WIN32 )
		if( WSAGetLastError() == ERROR_IO_PENDING )
#endif
		{
			DebugLog( __FILE__ , __LINE__ , "connect[%s:%d] inprocess" , p_http_session->p_forward_server->netaddr.ip , p_http_session->p_forward_server->netaddr.port );
			
#if ( defined __linux ) || ( defined __unix )
			/* 注册epoll写事件 */
			memset( & event , 0x00 , sizeof(struct epoll_event) );
			event.events = EPOLLOUT | EPOLLERR ;
			event.data.ptr = p_http_session ;
			nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_ADD , p_http_session->forward_netaddr.sock , & event ) ;
			if( nret == -1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , ERRNO );
				SetHttpSessionUnused_05( p_env , p_http_session );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
#endif
		}
		else
		{
			ErrorLog( __FILE__ , __LINE__ , "connect[%s:%d] failed , errno[%d]" , p_http_session->p_forward_server->netaddr.ip , p_http_session->p_forward_server->netaddr.port , ERRNO );
			return HTTP_SERVICE_UNAVAILABLE;
		}
	}
	else
	{
		DebugLog( __FILE__ , __LINE__ , "connect[%s:%d] ok" , p_http_session->p_forward_server->netaddr.ip , p_http_session->p_forward_server->netaddr.port );
		p_http_session->forward_flags = HTTPSESSION_FLAGS_CONNECTED ;
#if ( defined _WIN32 )
		setsockopt( p_http_session->forward_netaddr.sock , SOL_SOCKET , SO_UPDATE_CONNECT_CONTEXT , NULL , 0 );
#endif
				
		/* SSL握手 */
		if( p_http_session->p_virtualhost->forward_ssl_ctx )
		{
			p_http_session->forward_ssl = SSL_new( p_http_session->p_virtualhost->forward_ssl_ctx ) ;
			if( p_http_session->forward_ssl == NULL )
			{
				ErrorLog( __FILE__ , __LINE__ , "SSL_new failed , errno[%d]" , ERRNO );
				SetHttpSessionUnused_05( p_env , p_http_session );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			
			SSL_set_fd( p_http_session->forward_ssl , p_http_session->forward_netaddr.sock );
			
			SSL_set_connect_state( p_http_session->forward_ssl );
			p_http_session->forward_ssl_connected = 0 ;
			
			nret = OnConnectingSslForward( p_env , p_http_session ) ;
			if( nret == -1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "OnConnectingSslForward failed , errno[%d]" , ERRNO );
				SetHttpSessionUnused_05( p_env , p_http_session );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			
			return 0;
		}
		
		/* 复制HTTP请求 */
		request_base = GetHttpBufferBase( GetHttpRequestBuffer(p_http_session->http) , & request_len ) ;
		forward_b = GetHttpRequestBuffer( p_http_session->forward_http ) ;
		nret = MemcatHttpBuffer( forward_b , request_base , request_len ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "MemcatHttpBuffer failed , errno[%d]" , ERRNO );
			SetHttpSessionUnused_05( p_env , p_http_session );
			return HTTP_INTERNAL_SERVER_ERROR;
		}
		
#if ( defined __linux ) || ( defined __unix )
		/* 注册epoll写事件 */
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLOUT | EPOLLERR ;
		event.data.ptr = p_http_session ;
		nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_ADD , p_http_session->forward_netaddr.sock , & event ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , ERRNO );
			SetHttpSessionUnused_05( p_env , p_http_session );
			return HTTP_INTERNAL_SERVER_ERROR;
		}
#elif ( defined _WIN32 )
		/* 投递发送事件 */
		if( p_http_session->forward_ssl == NULL )
		{
			forward_b = GetHttpRequestBuffer( p_http_session->forward_http );
			buf.buf = GetHttpBufferBase( forward_b , NULL ) ;
			buf.len = GetHttpBufferLengthUnprocessed( forward_b ) ;
		}
		else
		{
			forward_b = GetHttpRequestBuffer( p_http_session->forward_http );
			SSL_write( p_http_session->forward_ssl , GetHttpBufferBase( forward_b , NULL ) , GetHttpBufferLength( forward_b ) );
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
				SetHttpSessionUnused_05( p_env , p_http_session );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
		}
#endif
	}
	
	return HTTP_OK;
}

int OnConnectingForward( struct HetaoEnv *p_env , struct HttpSession *p_http_session )
{
#if ( defined __linux ) || ( defined __unix )
	struct epoll_event	event ;
	int			error , code ;
#endif
	SOCKLEN_T		addr_len ;

	struct HttpBuffer	*b = NULL ;
#if ( defined _WIN32 )
	WSABUF			buf ;
	DWORD			dwFlags ;
#endif
	
	char			*request_base = NULL ;
	int			request_len ;
	struct HttpBuffer	*forward_b = NULL ;
	
	int			nret = 0 ;
	
	/* 检查非堵塞连接结果 */
#if ( defined __linux ) || ( defined __unix )
	addr_len = sizeof(int) ;
	code = getsockopt( p_http_session->forward_netaddr.sock , SOL_SOCKET , SO_ERROR , & error , & addr_len ) ;
	if( code < 0 || error )
#elif ( defined _WIN32 )
	/*
	addr_len = sizeof(struct sockaddr_in) ;
	nret = connect( p_http_session->forward_netaddr.sock , (struct sockaddr *) & (p_http_session->p_forward_server->netaddr.addr) , sizeof(struct sockaddr) ) ;
	if( nret == -1 && ERRNO == EISCONN )
	{
		;
	}
	else
	*/
	
	int			error ;
	int			code ;
	
	addr_len = sizeof(int) ;
	code = getsockopt( p_http_session->forward_netaddr.sock , SOL_SOCKET , SO_CONNECT_TIME , (char*) & error , & addr_len ) ;
	if( code != NO_ERROR || error == 0xFFFFFFFF )
	
#endif
        {
		ErrorLog( __FILE__ , __LINE__ , "connect[%s:%d] failed , errno[%d]" , p_http_session->p_forward_server->netaddr.ip , p_http_session->p_forward_server->netaddr.port , ERRNO );
		
#if ( defined __linux ) || ( defined __unix )
		epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_DEL , p_http_session->forward_netaddr.sock , NULL );
#endif
		SetHttpSessionUnused_02( p_env , p_http_session );
		
		p_http_session->p_forward_server->timestamp_to_valid = GETSECONDSTAMP + p_env->http_options__forward_disable ;
		
		while(1)
		{
			/* 选择转发服务端 */
			nret = SelectForwardAddress( p_env , p_http_session ) ;
			if( nret == HTTP_OK )
			{
				/* 连接转发服务端 */
				nret = ConnectForwardServer( p_env , p_http_session ) ;
				if( nret == HTTP_OK )
				{
					return 0;
				}
				else
				{
					ErrorLog( __FILE__ , __LINE__ , "SelectForwardAddress failed[%d] , errno[%d]" , nret , errno );
					
					/* 格式化响应头和体，用出错状态码 */
					nret = FormatHttpResponseStartLine( nret , p_http_session->http , 1 ) ;
					if( nret )
					{
						ErrorLog( __FILE__ , __LINE__ , "FormatHttpResponseStartLine failed[%d] , errno[%d]" , nret , ERRNO );
					}
					
					break;
				}
			}
			else
			{
				ErrorLog( __FILE__ , __LINE__ , "SelectForwardAddress failed[%d] , errno[%d]" , nret , ERRNO );
				
				/* 格式化响应头和体，用出错状态码 */
				nret = FormatHttpResponseStartLine( nret , p_http_session->http , 1 ) ;
				if( nret )
				{
					ErrorLog( __FILE__ , __LINE__ , "FormatHttpResponseStartLine failed[%d] , errno[%d]" , nret , ERRNO );
				}
				
				break;
			}
		}
		
		b = GetHttpResponseBuffer(p_http_session->http) ;
		DebugHexLog( __FILE__ , __LINE__ , GetHttpBufferBase(b,NULL) , GetHttpBufferLength(b) , "HttpResponseBuffer [%d]bytes" , GetHttpBufferLength(b) );
		
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
		p_http_session->flag = HTTPSESSION_FLAGS_SENDING ;
		
		/* 投递发送事件 */
		if( p_http_session->ssl == NULL )
		{
			b = GetHttpResponseBuffer( p_http_session->http );
			buf.buf = GetHttpBufferBase( b , NULL ) ;
			buf.len = GetHttpBufferLength( b ) ;
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
		
		return 0;
        }
	
	DebugLog( __FILE__ , __LINE__ , "connect2[%s:%d] ok" , p_http_session->p_forward_server->netaddr.ip , p_http_session->p_forward_server->netaddr.port );
	p_http_session->forward_flags = HTTPSESSION_FLAGS_CONNECTED ;
	
	/* SSL握手 */
	if( p_http_session->p_virtualhost->forward_ssl_ctx )
	{
		p_http_session->forward_ssl = SSL_new( p_http_session->p_virtualhost->forward_ssl_ctx ) ;
		if( p_http_session->forward_ssl == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "SSL_new failed , errno[%d]" , ERRNO );
			SetHttpSessionUnused_05( p_env , p_http_session );
			return 1;
		}
		
		SSL_set_fd( p_http_session->forward_ssl , p_http_session->forward_netaddr.sock );
		
		SSL_set_connect_state( p_http_session->forward_ssl );
		p_http_session->forward_ssl_connected = 0 ;
		
		nret = OnConnectingSslForward( p_env , p_http_session ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "OnConnectingSslForward failed , errno[%d]" , ERRNO );
			SetHttpSessionUnused_05( p_env , p_http_session );
			return 1;
		}
		
		return 0;
	}
	
	/* 复制HTTP请求 */
	request_base = GetHttpBufferBase( GetHttpRequestBuffer(p_http_session->http) , & request_len ) ;
	forward_b = GetHttpRequestBuffer( p_http_session->forward_http ) ;
	nret = MemcatHttpBuffer( forward_b , request_base , request_len ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , ERRNO );
		SetHttpSessionUnused_05( p_env , p_http_session );
		return 1;
	}
	
#if ( defined __linux ) || ( defined __unix )
	/* 注册epoll写事件 */
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLOUT | EPOLLERR ;
	event.data.ptr = p_http_session ;
	nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_MOD , p_http_session->forward_netaddr.sock , & event ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , ERRNO );
		SetHttpSessionUnused_05( p_env , p_http_session );
		return -1;
	}
#elif ( defined _WIN32 )
	p_http_session->flag = HTTPSESSION_FLAGS_SENDING ;
	
	/* 投递发送事件 */
	if( p_http_session->forward_ssl == NULL )
	{
		forward_b = GetHttpRequestBuffer( p_http_session->forward_http );
		buf.buf = GetHttpBufferBase( forward_b , NULL ) ;
		buf.len = GetHttpBufferLength( forward_b ) ;
	}
	else
	{
		forward_b = GetHttpRequestBuffer( p_http_session->forward_http );
		SSL_write( p_http_session->forward_ssl , GetHttpBufferBase( forward_b , NULL ) , GetHttpBufferLength( forward_b ) );
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
	else
	{
		InfoLog( __FILE__ , __LINE__ , "WSASend ok" );
	}
#endif
	
	return 0;
}
