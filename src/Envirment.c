/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int InitServerEnvirment( struct HetaoServer *p_server )
{
	int			i , j ;
	
	struct VirtualHost	*p_virtualhost = NULL ;
	
	struct NetAddress	*old_netaddr_array = NULL ;
	int			old_netaddr_array_count ;
	
	char			*port_str = NULL ;
	char			*p = NULL ;
	int			port ;
	struct NetAddress	*p_netaddr = NULL ;
	struct ListenSession	*p_listen_session = NULL ;
	
	struct MimeType		*p_mimetype = NULL ;
	char			*p_type = NULL ;
	
	int			nret = 0 ;
	
	/* 创建共享内存给工作进程组使用 */
	p_server->process_info_shmid = shmget( IPC_PRIVATE , sizeof(struct ProcessInfo) * p_server->p_config->worker_processes , IPC_CREAT | 0600 ) ;
	if( p_server->process_info_shmid == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "shmget failed , errno[%d]" , errno );
		return -1;
	}
	else
	{
		DebugLog( __FILE__ , __LINE__ , "shmget ok[%ld]" , p_server->process_info_shmid );
	}
	
	p_server->process_info_array = shmat( p_server->process_info_shmid , NULL , 0 ) ;
	if( p_server->process_info_array == (void*)-1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "shmat failed , errno[%d]" , errno );
		return -2;
	}
	else
	{
		DebugLog( __FILE__ , __LINE__ , "shmat[%ld] ok , [%ld]bytes" , p_server->process_info_shmid , sizeof(struct ProcessInfo) * p_server->p_config->worker_processes );
	}
	memset( p_server->process_info_array , 0x00 , sizeof(struct ProcessInfo) * p_server->p_config->worker_processes );
	
	/* 创建虚拟主机哈希表 */
	nret = InitVirtualHostHash( p_server ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "InitVirtualHostHash failed[%d]" , nret );
		return -1;
	}
	
	/* 注册所有虚拟主机 */
	p_server->virtualhost_count = 0 ;
	
	if( p_server->p_config->server.wwwroot[0] && p_server->p_config->server.access_log )
	{
		p_virtualhost = (struct VirtualHost *)malloc( sizeof(struct VirtualHost) ) ;
		if( p_virtualhost == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , errno );
			return -1;
		}
		memset( p_virtualhost , 0x00 , sizeof(struct VirtualHost) );
		
		strncpy( p_virtualhost->domain , p_server->p_config->server.domain , sizeof(p_virtualhost->domain)-1 );
		p_virtualhost->domain_len = strlen(p_virtualhost->domain) ;
		strncpy( p_virtualhost->wwwroot , p_server->p_config->server.wwwroot , sizeof(p_virtualhost->wwwroot)-1 );
		strncpy( p_virtualhost->index , p_server->p_config->server.index , sizeof(p_virtualhost->index)-1 );
		strncpy( p_virtualhost->access_log , p_server->p_config->server.access_log , sizeof(p_virtualhost->access_log)-1 );
		p_virtualhost->access_log_fd = OPEN( p_virtualhost->access_log , O_CREAT_WRONLY_APPEND ) ;
		if( p_virtualhost->access_log_fd == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "open access log[%s] failed , errno[%d]" , p_virtualhost->access_log , errno );
			return -1;
		}
		SetHttpCloseExec( p_virtualhost->access_log_fd );
		
		strncpy( p_virtualhost->forward_type , p_server->p_config->server.forward.forward_type , sizeof(p_virtualhost->forward_type)-1 );
		p_virtualhost->forward_type_len = strlen(p_virtualhost->forward_type) ;
		strncpy( p_virtualhost->forward_rule , p_server->p_config->server.forward.forward_rule , sizeof(p_virtualhost->forward_rule)-1 );
		if( p_virtualhost->forward_type[0] && p_virtualhost->forward_rule[0] && p_server->p_config->server.forward.forward_servers._forward_server_count > 0 )
		{
			struct ForwardServer	*p_forward_server = NULL ;
			
			INIT_LIST_HEAD( & (p_virtualhost->roundrobin_list.roundrobin_node) );
			
			for( j = 0 ; j < p_server->p_config->server.forward.forward_servers._forward_server_count ; j++ )
			{
				p_forward_server = (struct ForwardServer *)malloc( sizeof(struct ForwardServer) ) ;
				if( p_forward_server == NULL )
				{
					ErrorLog( __FILE__ , __LINE__ , "malloc failed[%d] , errno[%d]" , errno );
					return -1;
				}
				memset( p_forward_server , 0x00 , sizeof(sizeof(struct ForwardServer)) );
				strncpy( p_forward_server->netaddr.ip , p_server->p_config->server.forward.forward_servers.forward_server[j].ip , sizeof(p_forward_server->netaddr.ip)-1 );
				p_forward_server->netaddr.port = p_server->p_config->server.forward.forward_servers.forward_server[j].port ;
				SETNETADDRESS( p_forward_server->netaddr )
				
				DebugLog( __FILE__ , __LINE__ , "forward[%p]node[%p] server[%s:%d]" , p_forward_server , & (p_forward_server->roundrobin_node) , p_forward_server->netaddr.ip , p_forward_server->netaddr.port );
				
				list_add_tail( & (p_forward_server->roundrobin_node) , & (p_virtualhost->roundrobin_list.roundrobin_node) );
				AddLeastConnectionCountTreeNode( p_virtualhost , p_forward_server );
			}
		}
		
		nret = PushVirtualHostHashNode( p_server , p_virtualhost ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "PushVirtualHostHashNode[%s][%s] failed[%d] , errno[%d]" , p_virtualhost->domain , p_virtualhost->wwwroot , nret , errno );
			return -1;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "PushVirtualHostHashNode[%s][%s] ok" , p_virtualhost->domain , p_virtualhost->wwwroot , nret );
		}
		
		if( p_virtualhost->domain[0] == '\0' )
			p_server->p_virtualhost_default = p_virtualhost ;
		
		p_server->virtualhost_count++;
	}
	
	for( i = 0 ; i < p_server->p_config->servers._server_count ; i++ )
	{
		if( p_server->p_config->servers.server[i].wwwroot[0] == '\0' || p_server->p_config->servers.server[i].access_log[0] == '\0' )
			continue;
		
		p_virtualhost = (struct VirtualHost *)malloc( sizeof(struct VirtualHost) ) ;
		if( p_virtualhost == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , errno );
			return -1;
		}
		memset( p_virtualhost , 0x00 , sizeof(struct VirtualHost) );
		strncpy( p_virtualhost->domain , p_server->p_config->servers.server[i].domain , sizeof(p_virtualhost->domain)-1 );
		p_virtualhost->domain_len = strlen(p_virtualhost->domain) ;
		strncpy( p_virtualhost->wwwroot , p_server->p_config->servers.server[i].wwwroot , sizeof(p_virtualhost->wwwroot)-1 );
		strncpy( p_virtualhost->index , p_server->p_config->servers.server[i].index , sizeof(p_virtualhost->index)-1 );
		strncpy( p_virtualhost->access_log , p_server->p_config->servers.server[i].access_log , sizeof(p_virtualhost->access_log)-1 );
		p_virtualhost->access_log_fd = OPEN( p_virtualhost->access_log , O_CREAT_WRONLY_APPEND ) ;
		if( p_virtualhost->access_log_fd == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "open access log[%s] failed , errno[%d]" , p_virtualhost->access_log , errno );
			return -1;
		}
		SetHttpCloseExec( p_virtualhost->access_log_fd );
		
		strncpy( p_virtualhost->forward_type , p_server->p_config->servers.server[i].forward.forward_type , sizeof(p_virtualhost->forward_type)-1 );
		p_virtualhost->forward_type_len = strlen(p_virtualhost->forward_type) ;
		strncpy( p_virtualhost->forward_rule , p_server->p_config->servers.server[i].forward.forward_rule , sizeof(p_virtualhost->forward_rule)-1 );
		if( p_virtualhost->forward_rule[0] && p_server->p_config->servers.server[i].forward.forward_servers._forward_server_count > 0 )
		{
			struct ForwardServer	*p_forward_server = NULL ;
			
			INIT_LIST_HEAD( & (p_virtualhost->roundrobin_list.roundrobin_node) );
			
			for( j = 0 ; j < p_server->p_config->servers.server[j].forward.forward_servers._forward_server_count ; i++ )
			{
				p_forward_server = (struct ForwardServer *)malloc( sizeof(struct ForwardServer) ) ;
				if( p_forward_server == NULL )
				{
					ErrorLog( __FILE__ , __LINE__ , "malloc failed[%d] , errno[%d]" , errno );
					return -1;
				}
				memset( p_forward_server , 0x00 , sizeof(sizeof(struct ForwardServer)) );
				strncpy( p_forward_server->netaddr.ip , p_server->p_config->servers.server[i].forward.forward_servers.forward_server[j].ip , sizeof(p_forward_server->netaddr.ip)-1 );
				p_forward_server->netaddr.port = p_server->p_config->servers.server[i].forward.forward_servers.forward_server[j].port ;
				SETNETADDRESS( p_forward_server->netaddr )
				
				list_add_tail( & (p_forward_server->roundrobin_node) , & (p_virtualhost->roundrobin_list.roundrobin_node) );
				AddLeastConnectionCountTreeNode( p_virtualhost , p_forward_server );
			}
		}
		
		nret = PushVirtualHostHashNode( p_server , p_virtualhost ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "PushVirtualHostHashNode[%s][%s] failed[%d] , errno[%d]" , p_virtualhost->domain , p_virtualhost->wwwroot , nret , errno );
			return -1;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "PushVirtualHostHashNode[%s][%s] ok" , p_virtualhost->domain , p_virtualhost->wwwroot , nret );
		}
		
		if( p_virtualhost->domain[0] == '\0' )
			p_server->p_virtualhost_default = p_virtualhost ;
		
		p_server->virtualhost_count++;
	}
	
	/* 创建流类型哈希表 */
	nret = InitMimeTypeHash( p_server ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "InitMimeTypeHash failed[%d]" , nret );
		return -1;
	}
	
	/* 注册所有流类型 */
	for( i = 0 ; i < p_server->p_config->mime_types._mime_type_count ; i++ )
	{
		p_type = strtok( p_server->p_config->mime_types.mime_type[i].type , " \r" ) ;
		while( p_type )
		{
			p_mimetype = (struct MimeType *)malloc( sizeof(struct MimeType) ) ;
			if( p_mimetype == NULL )
			{
				ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , errno );
				return -1;
			}
			memset( p_mimetype , 0x00 , sizeof(struct MimeType) );
			
			strncpy( p_mimetype->type , p_type , sizeof(p_mimetype->type)-1 );
			p_mimetype->type_len = strlen(p_mimetype->type) ;
			strncpy( p_mimetype->mime , p_server->p_config->mime_types.mime_type[i].mime , sizeof(p_mimetype->mime)-1 );
			
			nret = PushMimeTypeHashNode( p_server , p_mimetype ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "PushMimeTypeHashNode failed[%d]" , nret );
				return -1;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "PushMimeTypeHashNode[%s][%s] ok" , p_mimetype->type , p_mimetype->mime );
			}
			
			p_type = strtok( NULL , " \t" ) ;
		}
	}
	
	/* 创建侦听会话链表首节点 */
	memset( & (p_server->listen_session_list) , 0x00 , sizeof(struct ListenSession) );
	INIT_LIST_HEAD( & (p_server->listen_session_list.list) );
	p_server->listen_session_count = 0 ;
	
	/* 创建网页缓存会话链表首节点 */
	memset( & (p_server->htmlcache_session_list) , 0x00 , sizeof(struct HtmlCacheSession) );
	INIT_LIST_HEAD( & (p_server->htmlcache_session_list.list) );
	p_server->htmlcache_session_count = 0 ;
	
	/* 创建空闲HTTP通讯会话链表首节点，预分配节点 */
	memset( & (p_server->http_session_unused_list) , 0x00 , sizeof(struct HttpSession) );
	INIT_LIST_HEAD( & (p_server->http_session_unused_list.list) );
	p_server->http_session_unused_count = 0 ;
	
	nret = IncreaseHttpSessions( p_server , INIT_HTTP_SESSION_COUNT ) ;
	if( nret )
		return nret;
	
	/* 如果是优雅重启，从环境变量中获得上一辈侦听信息 */
	nret = LoadOldListenSockets( & old_netaddr_array , & old_netaddr_array_count ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "LoadOldListenSockets failed[%d] , errno[%d]" , nret , errno );
		return -1;
	}
	
	port_str = strdup( p_server->p_config->listen.port ) ;
	if( port_str == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "strdup failed , errno[%d]" , errno );
		return -1;
	}
	p = strtok( port_str , "," ) ;
	while( p )
	{
		port = atoi(p) ;
		
		p_listen_session = (struct ListenSession *)malloc( sizeof(struct ListenSession) ) ;
		if( p_listen_session == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , errno );
			free( port_str );
			return -1;
		}
		memset( p_listen_session , 0x00 , sizeof(struct ListenSession) );
		
		p_listen_session->type = DATASESSION_TYPE_LISTEN ;
		
		list_add( & (p_listen_session->list) , & (p_server->listen_session_list.list) );
		
		p_netaddr = GetListener( old_netaddr_array , old_netaddr_array_count , p_server->p_config->listen.ip , port ) ;
		if( p_netaddr )
		{
			/* 上一辈侦听信息中有本次侦听相同地址 */
			memcpy( & (p_listen_session->netaddr) , p_netaddr , sizeof(struct NetAddress) );
			DebugLog( __FILE__ , __LINE__ , "[%s:%d] reuse #%d#" , p_server->p_config->listen.ip , port , p_listen_session->netaddr.sock );
			p_server->listen_session_count++;
		}
		else
		{
			/* 上一辈侦听信息中没有本次侦听相同地址，老老实实创建新的侦听端口 */
			p_listen_session->netaddr.sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
			if( p_listen_session->netaddr.sock == -1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "socket failed , errno[%d]" , errno );
				free( port_str );
				return -1;
			}
			
			SetHttpNonblock( p_listen_session->netaddr.sock );
			SetHttpReuseAddr( p_listen_session->netaddr.sock );
			
			strncpy( p_listen_session->netaddr.ip , p_server->p_config->listen.ip , sizeof(p_listen_session->netaddr.ip)-1 );
			p_listen_session->netaddr.port = port ;
			SETNETADDRESS( p_listen_session->netaddr )
			nret = bind( p_listen_session->netaddr.sock , (struct sockaddr *) & (p_listen_session->netaddr.addr) , sizeof(struct sockaddr) ) ;
			if( nret == -1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "bind[%s:%s] failed , errno[%d]" , p_server->p_config->listen.ip , p , errno );
				free( port_str );
				return -1;
			}
			
			nret = listen( p_listen_session->netaddr.sock , 10240 ) ;
			if( nret == -1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "listen failed , errno[%d]" , errno );
				free( port_str );
				return -1;
			}
			
			DebugLog( __FILE__ , __LINE__ , "[%s:%d] listen #%d#" , p_server->p_config->listen.ip , port , p_listen_session->netaddr.sock );
			p_server->listen_session_count++;
		}
		
		p = strtok( NULL , "," ) ;
	}
	
	free( port_str );
	
	/* 清理上一辈侦听信息 */
	CloseUnusedOldListeners( old_netaddr_array , old_netaddr_array_count );
	
	return 0;
}

void CleanServerEnvirment( struct HetaoServer *p_server )
{
	int			i ;
	struct list_head	*p_curr = NULL , *p_next = NULL ;
	struct HttpSession	*p_http_session = NULL ;
	struct ListenSession	*p_listen_session = NULL ;
	
	list_for_each_safe( p_curr , p_next , & (p_server->http_session_unused_list.list) )
	{
		p_http_session = container_of( p_curr , struct HttpSession , list ) ;
		DestroyHttpEnv( p_http_session->http );
	}
	
	for( i = 0 ; i < p_server->p_config->worker_processes ; i++ )
	{
		DebugLog( __FILE__ , __LINE__ , "[%d]close epoll_fd #%d#" , i , p_server->process_info_array[i].epoll_fd );
		close( p_server->process_info_array[i].epoll_fd );
	}
	
	DebugLog( __FILE__ , __LINE__ , "delete all listen_session" );
	list_for_each_safe( p_curr , p_next , & (p_server->listen_session_list.list) )
	{
		p_listen_session = container_of( p_curr , struct ListenSession , list ) ;
		list_del( p_curr );
		free( p_listen_session );
	}
	
	DebugLog( __FILE__ , __LINE__ , "close htmlcache_inotify_fd #%d#" , p_server->htmlcache_inotify_fd );
	close( p_server->htmlcache_inotify_fd );
	
	DebugLog( __FILE__ , __LINE__ , "shmdt and shmctl IPC_RMID" );
	shmdt( p_server->process_info_array );
	shmctl( p_server->process_info_shmid , IPC_RMID , NULL );
	
	CleanVirtualHostHash( p_server );
	
	CleanMimeTypeHash( p_server );
	
	free( p_server->p_config );
	
	return;
}

int SaveListenSockets( struct HetaoServer *p_server )
{
	unsigned int		env_value_size ;
	char			*env_value = NULL ;
	struct list_head	*p_curr = NULL ;
	struct ListenSession	*p_listen_session = NULL ;
	
	env_value_size = (1+p_server->listen_session_count)*(10+1) + 1 ;
	env_value = (char*)malloc( env_value_size ) ;
	if( env_value == NULL )
		return -1;
	memset( env_value , 0x00 , env_value_size );
	
	/* 先侦听信息数量 */
	SNPRINTF( env_value , env_value_size-1 , "%d|" , p_server->listen_session_count );
	
	list_for_each( p_curr , & (p_server->listen_session_list.list) )
	{
		p_listen_session = container_of( p_curr , struct ListenSession , list ) ;
		
		/* 每一个侦听信息 */
		SNPRINTF( env_value+strlen(env_value) , env_value_size-1-strlen(env_value) , "%d|" , p_listen_session->netaddr.sock );
	}
	
	/* 写入环境变量 */
	InfoLog( __FILE__ , __LINE__ , "setenv[%s][%s]" , HETAO_LISTEN_SOCKFDS , env_value );
	setenv( HETAO_LISTEN_SOCKFDS , env_value , 1 );
	
	free( env_value );
	
	return 0;
}

int LoadOldListenSockets( struct NetAddress **pp_old_netaddr_array , int *p_old_netaddr_array_count )
{
	char				*p_env_value = NULL ;
	char				*p_sockfd_count = NULL ;
	int				i ;
	struct NetAddress		*p_old_netaddr = NULL ;
	char				*p_sockfd = NULL ;
	SOCKLEN_T			addr_len = sizeof(struct sockaddr) ;
	
	int				nret = 0 ;
	
	p_env_value = getenv( HETAO_LISTEN_SOCKFDS ) ;
	InfoLog( __FILE__ , __LINE__ , "getenv[%s][%s]" , HETAO_LISTEN_SOCKFDS , p_env_value );
	if( p_env_value )
	{
		p_env_value = strdup( p_env_value ) ;
		if( p_env_value == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "strdup failed , errno[%d]" , errno );
			return -1;
		}
		
		/* 先解析侦听信息数量 */
		p_sockfd_count = strtok( p_env_value , "|" ) ;
		if( p_sockfd_count )
		{
			(*p_old_netaddr_array_count) = atoi(p_sockfd_count) ;
			if( (*p_old_netaddr_array_count) > 0 )
			{
				(*pp_old_netaddr_array) = (struct NetAddress *)malloc( sizeof(struct NetAddress) * (*p_old_netaddr_array_count) ) ;
				if( (*pp_old_netaddr_array) == NULL )
				{
					ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , errno );
					free( p_env_value );
					return -1;
				}
				memset( (*pp_old_netaddr_array) , 0x00 , sizeof(struct NetAddress) * (*p_old_netaddr_array_count) );
				
				/* 解析每一个侦听信息 */
				for( i = 0 , p_old_netaddr = (*pp_old_netaddr_array) ; i < (*p_old_netaddr_array_count) ; i++ , p_old_netaddr++ )
				{
					p_sockfd = strtok( NULL , "|" ) ;
					if( p_sockfd == NULL )
					{
						ErrorLog( __FILE__ , __LINE__ , "env[%s][%s] invalid" , HETAO_LISTEN_SOCKFDS , p_env_value );
						free( p_env_value );
						return -1;
					}
					
					p_old_netaddr->sock = atoi(p_sockfd) ;
					nret = getsockname( p_old_netaddr->sock , (struct sockaddr *) & (p_old_netaddr->addr) , & addr_len ) ;
					if( nret == -1 )
					{
						ErrorLog( __FILE__ , __LINE__ , "getsockname failed , errno[%d]" , errno );
						free( p_env_value );
						return -1;
					}
					
					p_old_netaddr->ip[sizeof(p_old_netaddr->ip)-1] = '\0' ;
					inet_ntop( AF_INET , &(p_old_netaddr->addr) , p_old_netaddr->ip , sizeof(p_old_netaddr->ip) );
					p_old_netaddr->port = (int)ntohs( p_old_netaddr->addr.sin_port ) ;
					
					DebugLog( __FILE__ , __LINE__ , "load [%s:%d]#%d#" , p_old_netaddr->ip , p_old_netaddr->port , p_old_netaddr->sock );
				}
			}
		}
		
		free( p_env_value );
	}
	
	return 0;
}

struct NetAddress *GetListener( struct NetAddress *old_netaddr_array , int old_netaddr_array_count , char *ip , int port )
{
	int			i ;
	struct NetAddress	*p_old_netaddr = NULL ;
	
	if( old_netaddr_array == NULL )
		return 0;
	
	for( i = 0 , p_old_netaddr = old_netaddr_array ; i < old_netaddr_array_count ; i++ , p_old_netaddr++ )
	{
		if( ( ip[0] == '\0' || STRCMP( p_old_netaddr->ip , == , "ip" ) ) && p_old_netaddr->port == port )
		{
			p_old_netaddr->ip[0] = '\0' ;
			return p_old_netaddr;
		}
	}
	
	return NULL;
}

int CloseUnusedOldListeners( struct NetAddress *p_old_netaddr_array , int old_netaddr_array_count )
{
	int			i ;
	struct NetAddress	*p_old_netaddr = NULL ;
	
	if( p_old_netaddr_array == NULL )
		return 0;
	
	for( i = 0 , p_old_netaddr = p_old_netaddr_array ; i < old_netaddr_array_count ; i++ , p_old_netaddr++ )
	{
		if( p_old_netaddr->ip[0] )
		{
			DebugLog( __FILE__ , __LINE__ , "close old #%d#" , p_old_netaddr->sock );
			close( p_old_netaddr->sock );
		}
	}
	
	free( p_old_netaddr_array );
	
	return 0;
}

