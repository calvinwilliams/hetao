/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int InitListenEnvirment( struct HetaoEnv *p_env , hetao_conf *p_conf , struct NetAddress *old_netaddr_array , int old_netaddr_array_count )
{
	int			k , i , j ;
	
	struct ListenSession	*p_listen_session = NULL ;
	struct NetAddress	*p_netaddr = NULL ;
	
	struct VirtualHost	*p_virtualhost = NULL ;
	
	struct RewriteUrl	*p_rewrite_url = NULL ;
	const char		*error_desc = NULL ;
	int			error_offset ;
	
	int			nret = 0 ;
	
	for( k = 0 ; k < p_conf->_listen_count ; k++ )
	{
		p_listen_session = (struct ListenSession *)malloc( sizeof(struct ListenSession) ) ;
		if( p_listen_session == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , ERRNO );
			return -1;
		}
		memset( p_listen_session , 0x00 , sizeof(struct ListenSession) );
		
		p_listen_session->type = DATASESSION_TYPE_LISTEN ;
		
		list_add( & (p_listen_session->list) , & (p_env->listen_session_list.list) );
		
#if ( defined __linux ) || ( defined __unix )
		p_netaddr = GetListener( old_netaddr_array , old_netaddr_array_count , p_conf->listen[k].ip , p_conf->listen[k].port ) ;
		if( p_netaddr )
		{
			/* 上一辈侦听信息中有本次侦听相同地址 */
			memcpy( & (p_listen_session->netaddr) , p_netaddr , sizeof(struct NetAddress) );
			DebugLog( __FILE__ , __LINE__ , "[%s:%d] reuse #%d#" , p_conf->listen[k].ip , p_conf->listen[k].port , p_listen_session->netaddr.sock );
			p_env->listen_session_count++;
		}
		else
		{
#endif
			/* 上一辈侦听信息中没有本次侦听相同地址，老老实实创建新的侦听端口 */
#if ( defined __linux ) || ( defined __unix )
			p_listen_session->netaddr.sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
			if( p_listen_session->netaddr.sock == -1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "socket failed , errno[%d]" , ERRNO );
				return -1;
			}
#elif ( defined _WIN32 )
			p_listen_session->netaddr.sock = WSASocket( AF_INET , SOCK_STREAM , 0 , NULL , 0 , WSA_FLAG_OVERLAPPED ) ;
			if( p_listen_session->netaddr.sock == -1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "socket failed , errno[%d]" , ERRNO );
				return -1;
			}
#endif
			
#if ( defined __linux ) || ( defined __unix )
			SetHttpNonblock( p_listen_session->netaddr.sock );
#endif
			SetHttpReuseAddr( p_listen_session->netaddr.sock );
			
			strncpy( p_listen_session->netaddr.ip , p_conf->listen[k].ip , sizeof(p_listen_session->netaddr.ip)-1 );
			p_listen_session->netaddr.port = p_conf->listen[k].port ;
			SETNETADDRESS( p_listen_session->netaddr )
			nret = bind( p_listen_session->netaddr.sock , (struct sockaddr *) & (p_listen_session->netaddr.addr) , sizeof(struct sockaddr) ) ;
			if( nret == -1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "bind[%s:%d] failed , errno[%d]" , p_conf->listen[k].ip , p_conf->listen[k].port , ERRNO );
				return -1;
			}
			
			nret = listen( p_listen_session->netaddr.sock , 10240 ) ;
			if( nret == -1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "listen failed , errno[%d]" , ERRNO );
				return -1;
			}
			
			DebugLog( __FILE__ , __LINE__ , "[%s:%d] listen #%d#" , p_conf->listen[k].ip , p_conf->listen[k].port , p_listen_session->netaddr.sock );
			p_env->listen_session_count++;
#if ( defined __linux ) || ( defined __unix )
		}
#endif
		
		/* 创建SSL环境 */
		if( p_conf->listen[k].ssl.certificate_file[0] )
		{
			if( p_env->init_ssl_env_flag == 0 )
			{
				SSL_library_init();
				OpenSSL_add_ssl_algorithms();
				SSL_load_error_strings();
				p_env->init_ssl_env_flag = 1 ;
			}
			
			p_listen_session->ssl_ctx = SSL_CTX_new( SSLv23_method() ) ;
			if( p_listen_session->ssl_ctx == NULL )
			{
				ErrorLog( __FILE__ , __LINE__ , "SSL_CTX_new failed , errno[%d]" , ERRNO );
				return -1;
			}
			
			nret = SSL_CTX_use_certificate_file( p_listen_session->ssl_ctx , p_conf->listen[k].ssl.certificate_file , SSL_FILETYPE_PEM ) ;
			if( nret <= 0 )
			{
				ErrorLog( __FILE__ , __LINE__ , "SSL_CTX_use_certificate_file failed , errno[%d]" , ERRNO );
				return -1;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "SSL_CTX_use_certificate_file[%s] ok" , p_conf->listen[k].ssl.certificate_file );
			}
			
			nret = SSL_CTX_use_PrivateKey_file( p_listen_session->ssl_ctx , p_conf->listen[k].ssl.certificate_key_file , SSL_FILETYPE_PEM ) ;
			if( nret <= 0 )
			{
				ErrorLog( __FILE__ , __LINE__ , "SSL_CTX_use_PrivateKey_file failed , errno[%d]" , ERRNO );
				return -1;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "SSL_CTX_use_PrivateKey_file[%s] ok" , p_conf->listen[k].ssl.certificate_key_file );
			}
		}
		
		/* 注册所有虚拟主机 */
		nret = InitVirtualHostHash( p_listen_session , p_conf->listen[k]._website_count ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "InitVirtualHostHash failed[%d]" , nret );
			return -1;
		}
		
		p_listen_session->virtualhost_count = 0 ;
		for( i = 0 ; i < p_conf->listen[k]._website_count ; i++ )
		{
			if( p_conf->listen[k].website[i].wwwroot[0] == '\0' )
				continue;
			
			p_virtualhost = (struct VirtualHost *)malloc( sizeof(struct VirtualHost) ) ;
			if( p_virtualhost == NULL )
			{
				ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , ERRNO );
				return -1;
			}
			memset( p_virtualhost , 0x00 , sizeof(struct VirtualHost) );
			p_virtualhost->type = DATASESSION_TYPE_HTMLCACHE ;
			strncpy( p_virtualhost->domain , p_conf->listen[k].website[i].domain , sizeof(p_virtualhost->domain)-1 );
			p_virtualhost->domain_len = strlen(p_virtualhost->domain) ;
			strncpy( p_virtualhost->wwwroot , p_conf->listen[k].website[i].wwwroot , sizeof(p_virtualhost->wwwroot)-1 );
			strncpy( p_virtualhost->index , p_conf->listen[k].website[i].index , sizeof(p_virtualhost->index)-1 );
			strncpy( p_virtualhost->access_log , p_conf->listen[k].website[i].access_log , sizeof(p_virtualhost->access_log)-1 );
			if(  p_virtualhost->access_log[0] )
			{
				p_virtualhost->access_log_fd = OPEN( p_virtualhost->access_log , O_CREAT_WRONLY_APPEND ) ;
				if( p_virtualhost->access_log_fd == -1 )
				{
					ErrorLog( __FILE__ , __LINE__ , "open access log[%s] failed , errno[%d]" , p_virtualhost->access_log , ERRNO );
					return -1;
				}
				SetHttpCloseExec( p_virtualhost->access_log_fd );
			}
			else
			{
				p_virtualhost->access_log_fd = -1 ;
			}
			
			/* 创建rewrite链表 */
			memset( & (p_virtualhost->rewrite_url_list) , 0x00 , sizeof(struct RewriteUrl) );
			INIT_LIST_HEAD( & (p_virtualhost->rewrite_url_list.rewriteurl_node) );
			
			if( p_conf->listen[k].website[i]._rewrite_count > 0 && p_env->new_url_re == NULL )
			{
				if( p_env->new_url_re == NULL )
				{
					p_env->new_url_re = pcre_compile( TEMPLATE_PATTERN , 0 , & error_desc , & error_offset , NULL ) ;
					if( p_env->new_url_re == NULL )
					{
						ErrorLog( __FILE__ , __LINE__ , "pcre_compile[%s] failed[%s][%d]" , TEMPLATE_PATTERN , error_desc , error_offset );
						return -1;
					}
					DebugLog( __FILE__ , __LINE__ , "create new_url pattern[%s]" , TEMPLATE_PATTERN );
				}
			}	
			
			for( j = 0 ; j < p_conf->listen[k].website[i]._rewrite_count ; j++ )
			{
				if( p_conf->listen[k].website[i].rewrite[j].pattern[0] == '\0' || p_conf->listen[k].website[i].rewrite[j].new_url[0] == '\0' )
				{
					ErrorLog( __FILE__ , __LINE__ , "rewrite url invalid , pattern[%s] new_url[%s]" , p_conf->listen[k].website[i].rewrite[j].pattern , p_conf->listen[k].website[i].rewrite[j].new_url );
					return -1;
				}
				
				p_rewrite_url = (struct RewriteUrl *)malloc( sizeof(struct RewriteUrl) ) ;
				if( p_rewrite_url == NULL )
				{
					ErrorLog( __FILE__ , __LINE__ , "malloc failed[%d] , errno[%d]" , ERRNO );
					return -1;
				}
				memset( p_rewrite_url , 0x00 , sizeof(struct RewriteUrl) );
				
				strcpy( p_rewrite_url->pattern , p_conf->listen[k].website[i].rewrite[j].pattern );
				strcpy( p_rewrite_url->new_url , p_conf->listen[k].website[i].rewrite[j].new_url );
				p_rewrite_url->new_url_len = strlen( p_rewrite_url->new_url ) ;
				
				p_rewrite_url->pattern_re = pcre_compile( p_rewrite_url->pattern , PCRE_MULTILINE , & error_desc , & error_offset , NULL ) ;
				if( p_rewrite_url->pattern_re == NULL )
				{
					ErrorLog( __FILE__ , __LINE__ , "pcre_compile[%s] failed[%s][%d]" , p_rewrite_url->pattern , error_desc , error_offset );
					return -1;
				}
				
				list_add_tail( & (p_rewrite_url->rewriteurl_node) , & (p_virtualhost->rewrite_url_list.rewriteurl_node) );
				DebugLog( __FILE__ , __LINE__ , "add rewrite[%s][%s]" , p_rewrite_url->pattern , p_rewrite_url->new_url );
			}
			
			/* 创建反向代理链表 */
			strncpy( p_virtualhost->forward_type , p_conf->listen[k].website[i].forward.forward_type , sizeof(p_virtualhost->forward_type)-1 );
			p_virtualhost->forward_type_len = strlen(p_virtualhost->forward_type) ;
			strncpy( p_virtualhost->forward_rule , p_conf->listen[k].website[i].forward.forward_rule , sizeof(p_virtualhost->forward_rule)-1 );
			
			if( p_conf->listen[k].website[i].forward.ssl.certificate_file[0] )
			{
				if( p_env->init_ssl_env_flag == 0 )
				{
					SSL_library_init();
					OpenSSL_add_ssl_algorithms();
					SSL_load_error_strings();
					p_env->init_ssl_env_flag = 1 ;
				}
				
				p_virtualhost->forward_ssl_ctx = SSL_CTX_new( SSLv23_method() ) ;
				if( p_virtualhost->forward_ssl_ctx == NULL )
				{
					ErrorLog( __FILE__ , __LINE__ , "SSL_CTX_new failed , errno[%d]" , ERRNO );
					return -1;
				}
				
				nret = SSL_CTX_use_certificate_file( p_virtualhost->forward_ssl_ctx , p_conf->listen[k].website[i].forward.ssl.certificate_file , SSL_FILETYPE_PEM ) ;
				if( nret <= 0 )
				{
					ErrorLog( __FILE__ , __LINE__ , "SSL_CTX_use_certificate_file failed , errno[%d]" , ERRNO );
					return -1;
				}
				else
				{
					DebugLog( __FILE__ , __LINE__ , "SSL_CTX_use_certificate_file[%s] ok" , p_conf->listen[k].website[i].forward.ssl.certificate_file );
				}
			}
			
			INIT_LIST_HEAD( & (p_virtualhost->roundrobin_list.roundrobin_node) );
			
			/* 注册反向代理应用服务器 */
			if( p_virtualhost->forward_rule[0] && p_conf->listen[k].website[i].forward._forward_server_count > 0 )
			{
				struct ForwardServer	*p_forward_server = NULL ;
				
				for( j = 0 ; j < p_conf->listen[k].website[i].forward._forward_server_count ; j++ )
				{
					p_forward_server = (struct ForwardServer *)malloc( sizeof(struct ForwardServer) ) ;
					if( p_forward_server == NULL )
					{
						ErrorLog( __FILE__ , __LINE__ , "malloc failed[%d] , errno[%d]" , ERRNO );
						return -1;
					}
					memset( p_forward_server , 0x00 , sizeof(struct ForwardServer) );
					strncpy( p_forward_server->netaddr.ip , p_conf->listen[k].website[i].forward.forward_server[j].ip , sizeof(p_forward_server->netaddr.ip)-1 );
					p_forward_server->netaddr.port = p_conf->listen[k].website[i].forward.forward_server[j].port ;
					SETNETADDRESS( p_forward_server->netaddr )
					
					list_add_tail( & (p_forward_server->roundrobin_node) , & (p_virtualhost->roundrobin_list.roundrobin_node) );
					AddLeastConnectionCountTreeNode( p_virtualhost , p_forward_server );
				}
			}
			
			nret = PushVirtualHostHashNode( p_listen_session , p_virtualhost ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "PushVirtualHostHashNode[%s][%s] failed[%d] , errno[%d]" , p_virtualhost->domain , p_virtualhost->wwwroot , nret , ERRNO );
				return -1;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "PushVirtualHostHashNode[%s][%s] ok" , p_virtualhost->domain , p_virtualhost->wwwroot , nret );
			}
			
			if( p_virtualhost->domain[0] == '\0' && p_listen_session->p_virtualhost_default == NULL )
				p_listen_session->p_virtualhost_default = p_virtualhost ;
			
			p_listen_session->virtualhost_count++;
		}
	}
	
	return 0;
}

