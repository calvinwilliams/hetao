/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

static signed char		g_exit_flag = 0 ;
signed char			g_second_elapse = 0 ;

#if ( defined __linux ) || ( defined __unix )

void *WorkerThread( void *pv )
{
	struct HetaoEnv		*p_env = (struct HetaoEnv *)pv ;
	
	struct epoll_event	event , *p_event = NULL ;
	struct epoll_event	events[ MAX_EPOLL_EVENTS ] ;
	int			i , j ;
	
	struct DataSession	*p_data_session = NULL ;
	struct list_head	*p_curr = NULL ;
	struct ListenSession	*p_listen_session = NULL ;
	struct HtmlCacheSession	*p_htmlcache_session = NULL ;
	struct HttpSession	*p_http_session = NULL ;
	
	int			nret = 0 ;
	
	InfoLog( __FILE__ , __LINE__ , "--- worker[%d] begin ---" , p_env->process_info_index );
	
	/* 如果需要吧绑定CPU，绑定之 */
	if( p_env->cpu_affinity )
	{
		BindCpuAffinity( p_env->process_info_index );
	}
	
	/* 创建多路复用池 */
	p_env->p_this_process_info->epoll_fd = epoll_create( 1024 ) ;
	if( p_env->p_this_process_info->epoll_fd == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "[%d]epoll_create failed , errno[%d]" , p_env->process_info_index , ERRNO );
		return NULL;
	}
	else
	{
		DebugLog( __FILE__ , __LINE__ , "[%d]epoll_create ok #%d#" , p_env->process_info_index , p_env->p_this_process_info->epoll_fd );
	}
	SetHttpCloseExec( p_env->p_this_process_info->epoll_fd );
	
	/* 注册管道事件 */
	p_env->pipe_session.type = DATASESSION_TYPE_PIPE ;
	
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLIN | EPOLLERR ;
	event.data.ptr = & (p_env->pipe_session) ;
	nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_ADD , p_env->p_this_process_info->pipe[0] , & event ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "epoll_ctl #%d# failed , errno[%d]" , p_env->p_this_process_info->epoll_fd , ERRNO );
		return NULL;
	}
	
	/* 注册网页缓存事件 */
	p_env->htmlcache_session.type = DATASESSION_TYPE_HTMLCACHE ;
	
	p_env->htmlcache_inotify_fd = inotify_init() ;
	if( p_env->htmlcache_inotify_fd == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "inotify_init failed , errno[%d]" , ERRNO );
		return NULL;
	}
	SetHttpCloseExec( p_env->htmlcache_inotify_fd );
	
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLIN | EPOLLERR ;
	event.data.ptr = & (p_env->htmlcache_session) ;
	nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_ADD , p_env->htmlcache_inotify_fd , & event ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "epoll_ctl failed , errno[%d]" , ERRNO );
		return NULL;
	}
	else
	{
		DebugLog( __FILE__ , __LINE__ , "epoll_ctl #%d# add inotify_fd #%d#" , p_env->p_this_process_info->epoll_fd , p_env->htmlcache_inotify_fd );
	}
	
	/* 如果配置了ACCEPT锁，或者是第一个工作进程，注册侦听端口事件 */
	if( p_env->accept_mutex == 0 || p_env->p_this_process_info == p_env->process_info_array )
	{
		list_for_each( p_curr , & (p_env->listen_session_list.list) )
		{
			p_listen_session = container_of( p_curr , struct ListenSession , list ) ;
			
			memset( & event , 0x00 , sizeof(struct epoll_event) );
			event.events = EPOLLIN | EPOLLERR ;
			event.data.ptr = p_listen_session ;
			nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_ADD , p_listen_session->netaddr.sock , & event ) ;
			if( nret == -1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "epoll_ctl add listen failed , errno[%d]" , ERRNO );
				return NULL;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "epoll_ctl #%d# add listen #%d#" , p_env->p_this_process_info->epoll_fd , p_listen_session->netaddr.sock );
			}
		}
	}
	
	/* 主工作循环 */
	while( g_exit_flag == 0 || p_env->http_session_used_count > 0 )
	{
		/* 等待epoll事件 */
		InfoLog( __FILE__ , __LINE__ , "[%d]epoll_wait #%d# ... [%d][%d][%d,%d]" , p_env->process_info_index , p_env->p_this_process_info->epoll_fd , p_env->listen_session_count , p_env->htmlcache_session_count , p_env->http_session_used_count , p_env->http_session_unused_count );
		memset( events , 0x00 , sizeof(events) );
		p_env->p_this_process_info->epoll_nfds = epoll_wait( p_env->p_this_process_info->epoll_fd , events , MAX_EPOLL_EVENTS , 1000 ) ;
		if( p_env->p_this_process_info->epoll_nfds == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "[%d]epoll_wait failed , errno[%d]" , p_env->process_info_index , ERRNO );
			return NULL;
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "[%d]epoll_wait #%d# return[%d]events" , p_env->process_info_index , p_env->p_this_process_info->epoll_fd , p_env->p_this_process_info->epoll_nfds );
		}
		
		/* 定时器线程点亮了秒漏事件，检查超时的HTTP通讯会话，关闭之 */
		if( g_second_elapse == 1 )
		{
			struct HttpSession	*p_http_session = NULL ;
			
			while(1)
			{
				p_http_session = GetExpireHttpSessionTimeoutTreeNode( p_env ) ;
				if( p_http_session == NULL )
					break;
				
				WarnLog( __FILE__ , __LINE__ , "SESSION TIMEOUT --------- client_ip[%s]" , p_http_session->netaddr.ip );
				SetHttpSessionUnused( p_env , p_http_session );
			}
			
			while(1)
			{
				p_http_session = GetExpireHttpSessionElapseTreeNode( p_env ) ;
				if( p_http_session == NULL )
					break;
				
				WarnLog( __FILE__ , __LINE__ , "SESSION ELAPSE --------- client_ip[%s]" , p_http_session->netaddr.ip );
				SetHttpSessionUnused( p_env , p_http_session );
			}
			
			g_second_elapse = 0 ;
		}
		
		/* 处理所有事件 */
		for( i = 0 , p_event = events ; i < p_env->p_this_process_info->epoll_nfds ; i++ , p_event++ )
		{
			p_data_session = p_event->data.ptr ;
			
			/* 如果是侦听端口事件 */
			if( p_data_session->type == DATASESSION_TYPE_LISTEN )
			{
				p_listen_session = (struct ListenSession *)p_data_session ;
				
				DebugLog( __FILE__ , __LINE__ , "[%d]DATASESSION_TYPE_LISTEN[%p] ---------" , p_env->process_info_index , p_data_session );
				
				if( p_event->events & EPOLLIN )
				{
					/* 有新的连接进来 */
					struct ProcessInfo	*p_process_info = NULL ;
					struct ProcessInfo	*p_process_info_with_min_balance = NULL ;
					
					DebugLog( __FILE__ , __LINE__ , "EPOLLIN" );
					
					nret = OnAcceptingSocket( p_env , p_listen_session ) ;
					if( nret > 0 )
					{
						WarnLog( __FILE__ , __LINE__ , "OnAcceptingSocket warn[%d] , errno[%d]" , nret , ERRNO );
					}
					else if( nret < 0 )
					{
						ErrorLog( __FILE__ , __LINE__ , "OnAcceptingSocket failed[%d] , errno[%d]" , nret , ERRNO );
						return NULL;
					}
					else
					{
						DebugLog( __FILE__ , __LINE__ , "OnAcceptingSocket ok" );
					}
					
					/* 如果打开了ACCEPT锁，或者工作进程数量大于一个 */
					if( p_env->accept_mutex == 1 && p_env->worker_processes > 1 )
					{
						/* 选择最近处理事件最少的工作进程 */
						p_process_info_with_min_balance = p_env->process_info_array ;
						DebugLog( __FILE__ , __LINE__ , "process[%d] epoll_nfds[%d]" , 0 , p_process_info_with_min_balance->epoll_nfds );
						for( j = 1 , p_process_info = p_env->process_info_array + 1 ; j < p_env->worker_processes ; j++ , p_process_info++ )
						{
							DebugLog( __FILE__ , __LINE__ , "process[%d] epoll_nfds[%d]" , j , p_process_info->epoll_nfds );
							if( p_process_info->epoll_nfds < p_process_info_with_min_balance->epoll_nfds )
								p_process_info_with_min_balance = p_process_info ;
						}
						DebugLog( __FILE__ , __LINE__ , "p_process_info_with_min_balance process[%d]" , p_process_info_with_min_balance-p_env->process_info_array );
						
						if( p_process_info_with_min_balance != p_env->p_this_process_info )
						{
							/* 后一轮接受新连接的任务就交给你了 */
							list_for_each( p_curr , & (p_env->listen_session_list.list) )
							{
								p_listen_session = container_of( p_curr , struct ListenSession , list ) ;
								
								nret = epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_DEL , p_listen_session->netaddr.sock , NULL ) ;
								if( nret == -1 )
								{
									ErrorLog( __FILE__ , __LINE__ , "[%d]epoll_ctl failed , errno[%d]" , p_env->process_info_index , ERRNO );
									return NULL;
								}
								else
								{
									DebugLog( __FILE__ , __LINE__ , "[%d]epoll_ctl #%d# del listen #%d#" , p_env->process_info_index , p_env->p_this_process_info->epoll_fd , p_listen_session->netaddr.sock );
								}
								
								memset( & event , 0x00 , sizeof(struct epoll_event) );
								event.events = EPOLLIN | EPOLLERR ;
								event.data.ptr = p_listen_session ;
								nret = epoll_ctl( p_process_info_with_min_balance->epoll_fd , EPOLL_CTL_ADD , p_listen_session->netaddr.sock , & event ) ;
								if( nret == -1 )
								{
									ErrorLog( __FILE__ , __LINE__ , "[%d]epoll_ctl failed , errno[%d]" , p_process_info_with_min_balance-p_env->process_info_array , ERRNO );
									return NULL;
								}
								else
								{
									DebugLog( __FILE__ , __LINE__ , "[%d]epoll_ctl #%d# add listen #%d#" , p_process_info_with_min_balance-p_env->process_info_array , p_process_info_with_min_balance->epoll_fd , p_listen_session->netaddr.sock );
								}
							}
						}
					}
				}
				else if( p_event->events & EPOLLERR )
				{
					DebugLog( __FILE__ , __LINE__ , "EPOLLERR" );
					
					ErrorLog( __FILE__ , __LINE__ , "listen_sock epoll EPOLLERR" );
					return NULL;
				}
				else
				{
					DebugLog( __FILE__ , __LINE__ , "EPOLL?" );
					
					ErrorLog( __FILE__ , __LINE__ , "listen_sock epoll event invalid[%d]" , p_event->events );
					return NULL;
				}
			}
			/* 如果是HTTP通讯事件 */
			else if( p_data_session->type == DATASESSION_TYPE_HTTP )
			{
				p_http_session = (struct HttpSession *)p_data_session ;
				
				DebugLog( __FILE__ , __LINE__ , "[%d]DATASESSION_TYPE_HTTP[%p] ---------" , p_env->process_info_index , p_data_session );
				
				if( p_event->events & EPOLLIN )
				{
					/* HTTP接收事件 */
					DebugLog( __FILE__ , __LINE__ , "EPOLLIN" );
					
					if( p_http_session->ssl && p_http_session->ssl_accepted == 0 )
					{
						nret = OnAcceptingSslSocket( p_env , p_http_session ) ;
						if( nret > 0 )
						{
							DebugLog( __FILE__ , __LINE__ , "OnAcceptingSslSocket done[%d]" , nret );
							SetHttpSessionUnused( p_env , p_http_session );
						}
						else if( nret < 0 )
						{
							ErrorLog( __FILE__ , __LINE__ , "OnAcceptingSslSocket failed[%d] , errno[%d]" , nret , ERRNO );
							return NULL;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "OnAcceptingSslSocket ok" );
						}
					}
					else if( p_http_session->forward_ssl && p_http_session->forward_ssl_connected == 0 )
					{
						nret = OnConnectingSslForward( p_env , p_http_session ) ;
						if( nret > 0 )
						{
							DebugLog( __FILE__ , __LINE__ , "OnConnectingSslForward done[%d]" , nret );
							SetHttpSessionUnused( p_env , p_http_session );
						}
						else if( nret < 0 )
						{
							ErrorLog( __FILE__ , __LINE__ , "OnConnectingSslForward failed[%d] , errno[%d]" , nret , ERRNO );
							return NULL;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "OnConnectingSslForward ok" );
						}
					}
					else if( p_http_session->forward_flags == 0 )
					{
						nret = OnReceivingSocket( p_env , p_http_session ) ;
						if( nret > 0 )
						{
							DebugLog( __FILE__ , __LINE__ , "OnReceivingSocket done[%d]" , nret );
							SetHttpSessionUnused( p_env , p_http_session );
						}
						else if( nret < 0 )
						{
							ErrorLog( __FILE__ , __LINE__ , "OnReceivingSocket failed[%d] , errno[%d]" , nret , ERRNO );
							return NULL;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "OnReceivingSocket ok" );
						}
					}
					else
					{
						nret = OnReceivingForward( p_env , p_http_session ) ;
						if( nret > 0 )
						{
							DebugLog( __FILE__ , __LINE__ , "OnReceivingForward done[%d]" , nret );
							SetHttpSessionUnused( p_env , p_http_session );
						}
						else if( nret < 0 )
						{
							ErrorLog( __FILE__ , __LINE__ , "OnReceivingForward failed[%d] , errno[%d]" , nret , ERRNO );
							return NULL;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "OnReceivingForward ok" );
						}
					}
				}
				else if( p_event->events & EPOLLOUT )
				{
					/* HTTP发送事件 */
					DebugLog( __FILE__ , __LINE__ , "EPOLLOUT" );
					
					if( p_http_session->ssl && p_http_session->ssl_accepted == 0 )
					{
						nret = OnAcceptingSslSocket( p_env , p_http_session ) ;
						if( nret > 0 )
						{
							DebugLog( __FILE__ , __LINE__ , "OnAcceptingSslSocket done[%d]" , nret );
							SetHttpSessionUnused( p_env , p_http_session );
						}
						else if( nret < 0 )
						{
							ErrorLog( __FILE__ , __LINE__ , "OnAcceptingSslSocket failed[%d] , errno[%d]" , nret , ERRNO );
							return NULL;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "OnAcceptingSslSocket ok" );
						}
					}
					else if( p_http_session->forward_ssl && p_http_session->forward_ssl_connected == 0 )
					{
						nret = OnConnectingSslForward( p_env , p_http_session ) ;
						if( nret > 0 )
						{
							DebugLog( __FILE__ , __LINE__ , "OnConnectingSslForward done[%d]" , nret );
							SetHttpSessionUnused( p_env , p_http_session );
						}
						else if( nret < 0 )
						{
							ErrorLog( __FILE__ , __LINE__ , "OnConnectingSslForward failed[%d] , errno[%d]" , nret , ERRNO );
							return NULL;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "OnConnectingSslForward ok" );
						}
					}
					else if( p_http_session->forward_flags == 0 )
					{
						nret = OnSendingSocket( p_env , p_http_session ) ;
						if( nret > 0 )
						{
							DebugLog( __FILE__ , __LINE__ , "OnSendingSocket done[%d]" , nret );
							SetHttpSessionUnused( p_env , p_http_session );
						}
						else if( nret < 0 )
						{
							ErrorLog( __FILE__ , __LINE__ , "OnSendingSocket failed[%d] , errno[%d]" , nret , ERRNO );
							return NULL;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "OnSendingSocket ok" );
						}
					}
					else
					{
						if( p_http_session->forward_flags == HTTPSESSION_FLAGS_CONNECTED )
						{
							nret = OnSendingForward( p_env , p_http_session ) ;
							if( nret > 0 )
							{
								DebugLog( __FILE__ , __LINE__ , "OnSendingForward done[%d]" , nret );
								SetHttpSessionUnused( p_env , p_http_session );
							}
							else if( nret < 0 )
							{
								ErrorLog( __FILE__ , __LINE__ , "OnSendingForward failed[%d] , errno[%d]" , nret , ERRNO );
								return NULL;
							}
							else
							{
								DebugLog( __FILE__ , __LINE__ , "OnSendingForward ok" );
							}
						}
						else if( p_http_session->forward_flags == HTTPSESSION_FLAGS_CONNECTING )
						{
							nret = OnConnectingForward( p_env , p_http_session ) ;
							if( nret > 0 )
							{
								DebugLog( __FILE__ , __LINE__ , "OnConnectingForward done[%d]" , nret );
								SetHttpSessionUnused( p_env , p_http_session );
							}
							else if( nret < 0 )
							{
								ErrorLog( __FILE__ , __LINE__ , "OnConnectingForward failed[%d] , errno[%d]" , nret , ERRNO );
								return NULL;
							}
							else
							{
								DebugLog( __FILE__ , __LINE__ , "OnConnectingForward ok" );
							}
						}
					}
				}
				else if( p_event->events & EPOLLRDHUP )
				{
					DebugLog( __FILE__ , __LINE__ , "EPOLLRDHUP" );
					
					if( p_http_session->forward_flags )
					{
						InfoLog( __FILE__ , __LINE__ , "http sock epoll EPOLLRDHUP" );
						SetHttpSessionUnused( p_env , p_http_session );
					}
				}
				else if( p_event->events & EPOLLERR )
				{
					DebugLog( __FILE__ , __LINE__ , "EPOLLERR" );
					
					if( p_http_session->forward_flags == 0 )
					{
						ErrorLog( __FILE__ , __LINE__ , "http sock epoll EPOLLERR" );
						SetHttpSessionUnused( p_env , p_http_session );
					}
					else
					{
						if( p_http_session->forward_flags == HTTPSESSION_FLAGS_CONNECTING )
						{
							nret = OnConnectingForward( p_env , p_http_session ) ;
							if( nret > 0 )
							{
								DebugLog( __FILE__ , __LINE__ , "OnConnectingForward done[%d]" , nret );
								SetHttpSessionUnused( p_env , p_http_session );
							}
							else if( nret < 0 )
							{
								ErrorLog( __FILE__ , __LINE__ , "OnConnectingForward failed[%d] , errno[%d]" , nret , ERRNO );
								return NULL;
							}
							else
							{
								DebugLog( __FILE__ , __LINE__ , "OnConnectingForward ok" );
							}
						}
					}
				}
				else
				{
					DebugLog( __FILE__ , __LINE__ , "EPOLL?" );
					
					ErrorLog( __FILE__ , __LINE__ , "http sock epoll event invalid[%d]" , p_event->events );
					SetHttpSessionUnused( p_env , p_http_session );
				}
			}
			/* 如果是网页缓存事件 */
			else if( p_data_session->type == DATASESSION_TYPE_HTMLCACHE )
			{
				p_htmlcache_session = (struct HtmlCacheSession *)p_data_session ;
				
				DebugLog( __FILE__ , __LINE__ , "[%d]DATASESSION_TYPE_HTMLCACHE[%p] ---------" , p_env->process_info_index , p_data_session );
				
				nret = HtmlCacheEventHander( p_env ) ;
				if( nret )
				{
					ErrorLog( __FILE__ , __LINE__ , "HtmlCacheEventHander failed[%d] , errno[%d]" , nret , ERRNO );
					return NULL;
				}
				else
				{
					DebugLog( __FILE__ , __LINE__ , "HtmlCacheEventHander ok" );
				}
			}
			/* 如果是管道事件 */
			else if( p_data_session->type == DATASESSION_TYPE_PIPE )
			{
				int	n ;
				char	ch = 0 ;
				
				DebugLog( __FILE__ , __LINE__ , "[%d]DATASESSION_TYPE_PIPE[%p] ---------" , p_env->process_info_index , p_data_session );
				
				n = read( p_env->p_this_process_info->pipe[0] , & ch , 1 ) ;
				InfoLog( __FILE__ , __LINE__ , "read pipe return[%d] , ch[%c]" , n , ch );
				if( n == 0 )
				{
					/* 管理进程发送给了优雅结束信号 */
					struct ListenSession	*p_listen_session = NULL ;
					
					struct epoll_event	*p_clean_event = NULL ;
					struct DataSession	*p_data_session = NULL ;
					
					/* 关闭所有侦听 */
					list_for_each_entry( p_listen_session , & (p_env->listen_session_list.list) , struct ListenSession , list )
					{
						epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_DEL , p_listen_session->netaddr.sock , NULL );
						close( p_listen_session->netaddr.sock );
						
						if( p_listen_session->ssl_ctx )
						{
							SSL_CTX_free( p_listen_session->ssl_ctx );
							p_listen_session->ssl_ctx = NULL ;
						}
					}
					
					/* 屏蔽所有后续侦听事件 */
					for( j = p_event-events+1 , p_clean_event = events+j ; j < p_env->p_this_process_info->epoll_nfds ; j++ , p_clean_event++ )
					{
						p_data_session = p_clean_event->data.ptr ;
						if( p_data_session->type == DATASESSION_TYPE_LISTEN )
							p_data_session->type = 0 ;
					}
					
					/* 关闭管道 */
					epoll_ctl( p_env->p_this_process_info->epoll_fd , EPOLL_CTL_DEL , p_env->p_this_process_info->pipe[0] , NULL );
					close( p_env->p_this_process_info->pipe[0] );
					
					g_exit_flag = 1 ;
				}
				else if( n > 0 )
				{
					if( ch == SIGNAL_REOPEN_LOG )
					{
						struct ListenSession	*p_listen_session = NULL ;
						struct VirtualHost	*p_virtualhost = NULL ;
						
						/* 管理进程发送给了重新打开日志信号 */
						CloseLogFile();
						
						list_for_each_entry( p_listen_session , & (p_env->listen_session_list.list) , struct ListenSession , list )
						{
							for( i = 0 ; i < p_listen_session->virtualhost_hashsize ; i++ )
							{
								hlist_for_each_entry( p_virtualhost , p_listen_session->virtualhost_hash+i , struct VirtualHost , virtualhost_node )
								{
									if( p_virtualhost->access_log_fd == -1 )
										continue;
									
									DebugLog( __FILE__ , __LINE__ , "close access_log[%s] #%d#" , p_virtualhost->access_log , p_virtualhost->access_log_fd );
									close( p_virtualhost->access_log_fd );
									
									p_virtualhost->access_log_fd = OPEN( p_virtualhost->access_log , O_CREAT_WRONLY_APPEND ) ;
									if( p_virtualhost->access_log_fd == -1 )
									{
										ErrorLog( __FILE__ , __LINE__ ,  "open access log[%s] failed , errno[%d]" , p_virtualhost->access_log , ERRNO );
										return NULL;
									}
									else
									{
										DebugLog( __FILE__ , __LINE__ ,  "open access log[%s] ok" , p_virtualhost->access_log );
									}
								}
							}
						}
					}
				}
				else
				{
					ErrorLog( __FILE__ , __LINE__ ,  "read pipe failed , errno[%d]" , ERRNO );
					return NULL;
				}
			}
		}
	}
	
	InfoLog( __FILE__ , __LINE__ , "--- worker[%d] exit ---" , p_env->process_info_index );
	
	return NULL;
}

#elif ( defined _WIN32 )

void *WorkerThread( void *pv )
{
	struct HetaoEnv		*p_env = (struct HetaoEnv *)pv ;
	
	DWORD			transfer_bytes ;
	int			ssl_bytes ;
	struct DataSession	*p_data_session = NULL ;
	struct ListenSession	*p_listen_session = NULL ;
	struct HttpSession	*p_http_session = NULL ;
	
	int			i ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct VirtualHost	*p_virtualhost = NULL ;
	DWORD			dwByteRet ;
	
	struct HttpBuffer	*b = NULL ;
	
	int			nret = 0 ;
	BOOL			bret = TRUE ;
	HANDLE			hret = NULL ;
	
	SetLogFile( p_env->error_log );
	SetLogLevel( p_env->log_level );
	SETPID
	SETTID
	UPDATEDATETIMECACHEFIRST
	InfoLog( __FILE__ , __LINE__ , "--- worker[%d] begin ---" , p_env->process_info_index );
	
	/* 创建完成端口 */
	p_env->iocp = CreateIoCompletionPort( INVALID_HANDLE_VALUE, 0, 0, 0 ) ;
	if( p_env->iocp == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "CreateIoCompletionPort failed , errno[%d]" , ERRNO );
		return NULL;
	}
	else
	{
		DebugLog( __FILE__ , __LINE__ , "CreateIoCompletionPort ok" );
	}
	
	/* 装载所有侦听的所有虚拟主机 */
	list_for_each_entry( p_listen_session , & (p_env->listen_session_list.list) , struct ListenSession , list )
	{
		/* 得到AcceptEx函数指针 */
		{
			GUID	accept_ex_guid = WSAID_ACCEPTEX ;
			DWORD	bytes = 0 ;
			nret = WSAIoctl( p_listen_session->netaddr.sock , SIO_GET_EXTENSION_FUNCTION_POINTER , & accept_ex_guid , sizeof(accept_ex_guid) , & (p_listen_session->lpfnAcceptEx) , sizeof(p_listen_session->lpfnAcceptEx) , & bytes , NULL , NULL ) ;
			if( nret == SOCKET_ERROR )
			{
				ErrorLog( __FILE__ , __LINE__ , "WSAIoctl failed , errno[%d]" , ERRNO );
				return NULL;
			}
		}
		
		/* 得到ConnectEx函数指针 */
		{
			GUID	connect_ex_guid = WSAID_CONNECTEX ;
			DWORD	bytes = 0 ;
			nret = WSAIoctl( p_listen_session->netaddr.sock , SIO_GET_EXTENSION_FUNCTION_POINTER , & connect_ex_guid , sizeof(connect_ex_guid) , & (p_env->lpfnConnectEx) , sizeof(p_env->lpfnConnectEx) , & bytes , NULL , NULL ) ;
			if( nret == SOCKET_ERROR )
			{
				ErrorLog( __FILE__ , __LINE__ , "WSAIoctl failed , errno[%d]" , ERRNO );
				return NULL;
			}
		}
		
		p_listen_session->accept_socket = WSASocket( AF_INET , SOCK_STREAM , 0 , NULL , 0 , WSA_FLAG_OVERLAPPED ) ;
		if( p_listen_session->accept_socket == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "WSASocket failed , errno[%d]" , ERRNO );
			return NULL;
		}
		
		hret = CreateIoCompletionPort( (HANDLE)(p_listen_session->netaddr.sock) , p_env->iocp , (DWORD)p_listen_session , 0 ) ;
		if( hret == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "CreateIoCompletionPort failed , errno[%d]" , ERRNO );
			return NULL;
		}
		
		/* 投递accept事件 */
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
				return NULL;
			}
		}
		
		/* 投递目录变动事件 */
		for( i = 0 , p_hlist_head = p_listen_session->virtualhost_hash ; i < p_listen_session->virtualhost_hashsize ; i++ , p_hlist_head++ )
		{
			hlist_for_each_entry( p_virtualhost , p_hlist_head , struct VirtualHost , virtualhost_node )
			{
				p_virtualhost->directory_changes_handler = CreateFile( p_virtualhost->wwwroot , GENERIC_READ | GENERIC_WRITE , FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE , NULL , OPEN_EXISTING , FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED , NULL ) ;
				if( p_virtualhost->directory_changes_handler == INVALID_HANDLE_VALUE )
				{
					ErrorLog( __FILE__ , __LINE__ , "CreateFile[%s] failed , errno[%d]" , p_virtualhost->wwwroot , ERRNO );
					return NULL;
				}
				
				hret = CreateIoCompletionPort( p_virtualhost->directory_changes_handler , p_env->iocp , (DWORD)p_virtualhost , 0 ) ;
				if( hret == NULL )
				{
					ErrorLog( __FILE__ , __LINE__ , "CreateIoCompletionPort failed , errno[%d]" , ERRNO );
					return NULL;
				}
				
				bret = ReadDirectoryChangesW( p_virtualhost->directory_changes_handler , p_virtualhost->directory_changes_buffer , sizeof(p_virtualhost->directory_changes_buffer) , TRUE , FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SECURITY , & dwByteRet , & (p_virtualhost->overlapped) , NULL ) ;
				if( bret == FALSE )
				{
					ErrorLog( __FILE__ , __LINE__ , "ReadDirectoryChangesW[%s] failed , errno[%d]" , p_virtualhost->wwwroot , ERRNO );
					return NULL;
				}
			}
		}
	}
	
	while(1)
	{
		InfoLog( __FILE__ , __LINE__ , "[%d]GetQueuedCompletionStatus ... [%d][%d][%d,%d]" , p_env->process_info_index , p_env->listen_session_count , p_env->htmlcache_session_count , p_env->http_session_used_count , p_env->http_session_unused_count );
		bret = GetQueuedCompletionStatus( p_env->iocp , & transfer_bytes , (LPDWORD) & p_data_session , (LPOVERLAPPED *) & p_data_session , 1000 ) ;
		if( bret == FALSE && ERRNO != WAIT_TIMEOUT )
		{
			WarnLog( __FILE__ , __LINE__ , "[%d]GetQueuedCompletionStatus failed , errno[%d]" , p_env->process_info_index , ERRNO );
			continue;
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "[%d]GetQueuedCompletionStatus ok [%d][%d][%d,%d]" , p_env->process_info_index , p_env->listen_session_count , p_env->htmlcache_session_count , p_env->http_session_used_count , p_env->http_session_unused_count );
		}
		
		/* 定时器线程点亮了秒漏事件，检查超时的HTTP通讯会话，关闭之 */
		if( g_second_elapse == 1 )
		{
			struct HttpSession	*p_http_session = NULL ;
			
			while(1)
			{
				p_http_session = GetExpireHttpSessionTimeoutTreeNode( p_env ) ;
				if( p_http_session == NULL )
					break;
				
				WarnLog( __FILE__ , __LINE__ , "SESSION TIMEOUT --------- client_ip[%s]" , p_http_session->netaddr.ip );
				SetHttpSessionUnused( p_env , p_http_session );
			}
			
			while(1)
			{
				p_http_session = GetExpireHttpSessionElapseTreeNode( p_env ) ;
				if( p_http_session == NULL )
					break;
				
				WarnLog( __FILE__ , __LINE__ , "SESSION ELAPSE --------- client_ip[%s]" , p_http_session->netaddr.ip );
				SetHttpSessionUnused( p_env , p_http_session );
			}
			
			g_second_elapse = 0 ;
		}
		
		if( p_data_session == NULL )
			continue;
		if( bret == FALSE && ERRNO == WAIT_TIMEOUT )
			continue;
		
		/* 处理投递事件 */
		switch( p_data_session->type )
		{
			case DATASESSION_TYPE_LISTEN:
				DebugLog( __FILE__ , __LINE__ , "DATASESSION_TYPE_LISTEN" );
				p_listen_session = (struct ListenSession *)p_data_session ;
				
				nret = OnAcceptingSocket( p_env , p_listen_session ) ;
				if( nret )
				{
					FatalLog( __FILE__ , __LINE__ , "OnAcceptingSocket failed , errno[%d]" , ERRNO );
					return NULL;
				}
				
				break;
			case DATASESSION_TYPE_HTTP:
				DebugLog( __FILE__ , __LINE__ , "DATASESSION_TYPE_HTTP" );
				p_http_session = (struct HttpSession *)p_data_session ;
				
				if( p_http_session->forward_flags == 0 )
				{
					if( p_http_session->ssl && p_http_session->ssl_accepted == 0 )
					{
						nret = OnAcceptingSslSocket( p_env , p_http_session ) ;
						if( nret > 0 )
						{
							InfoLog( __FILE__ , __LINE__ , "OnAcceptingSslSocket done" );
							SetHttpSessionUnused( p_env , p_http_session );
						}
						else if( nret < 0 )
						{
							ErrorLog( __FILE__ , __LINE__ , "OnAcceptingSslSocket failed , errno[%d]" , ERRNO );
							SetHttpSessionUnused( p_env , p_http_session );
							return NULL;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "OnAcceptingSslSocket ok" );
						}
					}
					else if( p_http_session->flag == HTTPSESSION_FLAGS_RECEIVING )
					{
						if( transfer_bytes > 0 )
						{
							DebugLog( __FILE__ , __LINE__ , "socket received[%d]bytes" , transfer_bytes );
							
							b = GetHttpRequestBuffer(p_http_session->http) ;
							if( p_http_session->ssl == NULL )
							{
								OffsetHttpBufferFillPtr( b , transfer_bytes );
								if( GetHttpBufferLengthUnfilled( b ) <= 0 )
								{
									nret = ReallocHttpBuffer( b , -1 ) ;
									if( nret )
									{
										ErrorLog( __FILE__ , __LINE__ , "ReallocHttpBuffer failed , errno[%d]" , ERRNO );
										SetHttpSessionUnused( p_env , p_http_session );
										return NULL;
									}
								}
							}
							else
							{
								BIO_write( p_http_session->in_bio , p_http_session->in_bio_buffer , transfer_bytes );
								ssl_bytes = SSL_read( p_http_session->ssl , p_http_session->out_bio_buffer , sizeof(p_http_session->out_bio_buffer)-1 ) ;
								if( ssl_bytes == -1 )
								{
									ErrorLog( __FILE__ , __LINE__ , "SSL_read failed , errno[%d]" , ERRNO );
									SetHttpSessionUnused( p_env , p_http_session );
									continue;
								}
								MemcatHttpBuffer( b , p_http_session->out_bio_buffer , ssl_bytes );
							}
							
							nret = OnReceivingSocket( p_env , p_http_session ) ;
							if( nret > 0 )
							{
								InfoLog( __FILE__ , __LINE__ , "OnReceivingSocket done" );
								SetHttpSessionUnused( p_env , p_http_session );
							}
							else if( nret < 0 )
							{
								ErrorLog( __FILE__ , __LINE__ , "OnReceivingSocket failed , errno[%d]" , ERRNO );
								SetHttpSessionUnused( p_env , p_http_session );
								return NULL;
							}
							else
							{
								DebugLog( __FILE__ , __LINE__ , "OnReceivingSocket ok" );
							}
						}
						else
						{
							InfoLog( __FILE__ , __LINE__ , "http socket closed on receiving" );
							SetHttpSessionUnused( p_env , p_http_session );
						}
					}
					else if( p_http_session->flag == HTTPSESSION_FLAGS_SENDING )
					{
						if( transfer_bytes > 0 )
						{
							DebugLog( __FILE__ , __LINE__ , "socket sended[%d]bytes" , transfer_bytes );
							
							if( p_http_session->ssl == NULL )
							{
								if( GetHttpBufferLengthUnprocessed( GetHttpResponseBuffer(p_http_session->http) ) > 0 )
									OffsetHttpBufferProcessPtr( GetHttpResponseBuffer(p_http_session->http) , transfer_bytes );
								else
									OffsetHttpBufferProcessPtr( GetHttpAppendBuffer(p_http_session->http) , transfer_bytes );
							}
							
							nret = OnSendingSocket( p_env , p_http_session ) ;
							if( nret > 0 )
							{
								InfoLog( __FILE__ , __LINE__ , "OnSendingSocket done" );
								SetHttpSessionUnused( p_env , p_http_session );
							}
							else if( nret < 0 )
							{
								ErrorLog( __FILE__ , __LINE__ , "OnSendingSocket failed , errno[%d]" , ERRNO );
								SetHttpSessionUnused( p_env , p_http_session );
								return NULL;
							}
							else
							{
								DebugLog( __FILE__ , __LINE__ , "OnSendingSocket ok" );
							}
						}
						else
						{
							InfoLog( __FILE__ , __LINE__ , "http socket closed on sending" );
							SetHttpSessionUnused( p_env , p_http_session );
						}
					}
				}
				else if( p_http_session->forward_flags == HTTPSESSION_FLAGS_CONNECTING )
				{
					nret = OnConnectingForward( p_env , p_http_session ) ;
					if( nret > 0 )
					{
						InfoLog( __FILE__ , __LINE__ , "OnConnectingForward done" );
						SetHttpSessionUnused( p_env , p_http_session );
					}
					else if( nret < 0 )
					{
						ErrorLog( __FILE__ , __LINE__ , "OnConnectingForward failed , errno[%d]" , ERRNO );
						SetHttpSessionUnused( p_env , p_http_session );
						return NULL;
					}
					else
					{
						DebugLog( __FILE__ , __LINE__ , "OnConnectingForward ok" );
					}
				}
				else if( p_http_session->forward_flags == HTTPSESSION_FLAGS_CONNECTED )
				{
					if( p_http_session->forward_ssl && p_http_session->forward_ssl_connected == 0 )
					{
						nret = OnConnectingSslForward( p_env , p_http_session ) ;
						if( nret > 0 )
						{
							InfoLog( __FILE__ , __LINE__ , "OnConnectingSslForward done" );
							SetHttpSessionUnused( p_env , p_http_session );
						}
						else if( nret < 0 )
						{
							ErrorLog( __FILE__ , __LINE__ , "OnConnectingSslForward failed , errno[%d]" , ERRNO );
							SetHttpSessionUnused( p_env , p_http_session );
							return NULL;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "OnConnectingSslForward ok" );
						}
					}
					else if( p_http_session->flag == HTTPSESSION_FLAGS_SENDING )
					{
						if( transfer_bytes > 0 )
						{
							DebugLog( __FILE__ , __LINE__ , "socket sended[%d]bytes" , transfer_bytes );
							
							if( p_http_session->forward_ssl == NULL )
							{
								OffsetHttpBufferProcessPtr( GetHttpRequestBuffer(p_http_session->forward_http) , transfer_bytes );
							}
							
							nret = OnSendingForward( p_env , p_http_session ) ;
							if( nret > 0 )
							{
								InfoLog( __FILE__ , __LINE__ , "OnSendingForward done" );
								SetHttpSessionUnused( p_env , p_http_session );
							}
							else if( nret < 0 )
							{
								ErrorLog( __FILE__ , __LINE__ , "OnSendingForward failed , errno[%d]" , ERRNO );
								SetHttpSessionUnused( p_env , p_http_session );
								return NULL;
							}
							else
							{
								DebugLog( __FILE__ , __LINE__ , "OnSendingForward ok" );
							}
						}
						else
						{
							InfoLog( __FILE__ , __LINE__ , "http socket closed on sending" );
							SetHttpSessionUnused( p_env , p_http_session );
						}
					}
					else if( p_http_session->flag == HTTPSESSION_FLAGS_RECEIVING )
					{
						if( transfer_bytes > 0 )
						{
							DebugLog( __FILE__ , __LINE__ , "socket received[%d]bytes" , transfer_bytes );
							
							b = GetHttpResponseBuffer(p_http_session->forward_http) ;
							if( p_http_session->forward_ssl == NULL )
							{
								OffsetHttpBufferFillPtr( b , transfer_bytes );
								if( GetHttpBufferLengthUnfilled( b ) <= 0 )
								{
									nret = ReallocHttpBuffer( b , -1 ) ;
									if( nret )
									{
										ErrorLog( __FILE__ , __LINE__ , "ReallocHttpBuffer failed , errno[%d]" , ERRNO );
										SetHttpSessionUnused( p_env , p_http_session );
										return NULL;
									}
								}
							}
							else
							{
								BIO_write( p_http_session->forward_in_bio , p_http_session->forward_in_bio_buffer , transfer_bytes );
								transfer_bytes = SSL_read( p_http_session->forward_ssl , p_http_session->forward_out_bio_buffer , sizeof(p_http_session->forward_out_bio_buffer)-1 ) ;
								MemcatHttpBuffer( b , p_http_session->forward_out_bio_buffer , transfer_bytes );
							}
							
							nret = OnReceivingForward( p_env , p_http_session ) ;
							if( nret > 0 )
							{
								InfoLog( __FILE__ , __LINE__ , "OnReceivingForward done" );
								SetHttpSessionUnused( p_env , p_http_session );
							}
							else if( nret < 0 )
							{
								ErrorLog( __FILE__ , __LINE__ , "OnReceivingForward failed , errno[%d]" , ERRNO );
								SetHttpSessionUnused( p_env , p_http_session );
								return NULL;
							}
							else
							{
								DebugLog( __FILE__ , __LINE__ , "OnReceivingForward ok" );
							}
						}
						else
						{
							InfoLog( __FILE__ , __LINE__ , "http forward socket closed on receiving" );
							SetHttpSessionUnused( p_env , p_http_session );
						}
					}
					else
					{
						ErrorLog( __FILE__ , __LINE__ , "p_http_session->flag[%d] invalid" , p_http_session->flag );
					}
				}
				else
				{
					FatalLog( __FILE__ , __LINE__ , "Unknow p_http_session->forward_flags[0x%X]" , p_http_session->forward_flags );
					return NULL;
				}
				
				break;
			case DATASESSION_TYPE_HTMLCACHE:
				DebugLog( __FILE__ , __LINE__ , "DATASESSION_TYPE_HTMLCACHE" );
				p_virtualhost = (struct VirtualHost *)p_data_session ;
				
				nret = DirectoryWatcherEventHander( p_env , p_virtualhost ) ;
				if( nret )
				{
					FatalLog( __FILE__ , __LINE__ , "DirectoryWatcherEventHander failed , errno[%d]" , ERRNO );
					return NULL;
				}
				
				bret = ReadDirectoryChangesW( p_virtualhost->directory_changes_handler , p_virtualhost->directory_changes_buffer , sizeof(p_virtualhost->directory_changes_buffer) , TRUE , FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SECURITY , & dwByteRet , & (p_virtualhost->overlapped) , NULL ) ;
				if( bret == FALSE )
				{
					FatalLog( __FILE__ , __LINE__ , "ReadDirectoryChangesW failed , errno[%d]" , ERRNO );
					return NULL;
				}
				
				break;
			default :
				FatalLog( __FILE__ , __LINE__ , "Unknow event type[%c]" , p_data_session->type );
				return NULL;
		}
	}
	
	return NULL;
}

#endif
