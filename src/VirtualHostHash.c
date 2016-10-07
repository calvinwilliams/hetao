/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int InitVirtualHostHash( struct ListenSession *p_listen_session , int count )
{
	int			i ;
	struct hlist_head	*p_hlist_head = NULL ;
	
	p_listen_session->virtualhost_hashsize = count * 2 ;
	p_listen_session->virtualhost_hash = (struct hlist_head *)malloc( sizeof(struct hlist_head) * p_listen_session->virtualhost_hashsize ) ;
	if( p_listen_session->virtualhost_hash == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , ERRNO );
		return -1;
	}
	memset( p_listen_session->virtualhost_hash , 0x00 , sizeof(struct hlist_head) * p_listen_session->virtualhost_hashsize );
	
	for( i = 0 , p_hlist_head = p_listen_session->virtualhost_hash ; i < p_listen_session->virtualhost_hashsize ; i++ , p_hlist_head++ )
	{
		INIT_HLIST_HEAD( p_hlist_head );
	}
	
	return 0;
}

void CleanVirtualHostHash( struct ListenSession *p_listen_session )
{
	int			i ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct hlist_node	*curr = NULL , *next = NULL ;
	struct VirtualHost	*p_virtualhost = NULL ;
	struct RewriteUrl	*p_rewrite_url = NULL ;
	struct RewriteUrl	*p_next_rewrite_url = NULL ;
	
	for( i = 0 , p_hlist_head = p_listen_session->virtualhost_hash ; i < p_listen_session->virtualhost_hashsize ; i++ , p_hlist_head++ )
	{
		hlist_for_each_safe( curr , next , p_hlist_head )
		{
			hlist_del( curr );
			p_virtualhost = container_of(curr,struct VirtualHost,virtualhost_node) ;
			list_for_each_entry_safe( p_rewrite_url , p_next_rewrite_url , & (p_virtualhost->rewrite_url_list.rewriteurl_node) , struct RewriteUrl , rewriteurl_node )
			{
				free( p_rewrite_url->pattern_re );
			}
			if( p_virtualhost->forward_ssl_ctx )
			{
				SSL_CTX_free( p_virtualhost->forward_ssl_ctx );
				p_virtualhost->forward_ssl_ctx = NULL ;
			}
#if ( defined _WIN32 )
			CloseHandle( p_virtualhost->directory_changes_handler );
#endif
			free( p_virtualhost );
		}
	}
	
	free( p_listen_session->virtualhost_hash );
	p_listen_session->virtualhost_hash = NULL ;
	
	return;
}

int PushVirtualHostHashNode( struct ListenSession *p_listen_session , struct VirtualHost *p_virtualhost )
{
	int			index ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct VirtualHost	*p ;
	
	index = CalcHash(p_virtualhost->domain,p_virtualhost->domain_len) % (p_listen_session->virtualhost_hashsize) ;
	DebugLog( __FILE__ , __LINE__ , "VirtualHost[%d]=CalcHash[%.*s][%d]" , index , p_virtualhost->domain_len , p_virtualhost->domain , p_listen_session->virtualhost_hashsize );
	p_hlist_head = p_listen_session->virtualhost_hash + index ;
	hlist_for_each_entry( p , p_hlist_head , struct VirtualHost , virtualhost_node )
	{
		if( STRCMP( p->domain , == , p_virtualhost->domain ) )
			return 1;
	}
	hlist_add_head( & (p_virtualhost->virtualhost_node) , p_hlist_head );
	
	return 0;
}

struct VirtualHost *QueryVirtualHostHashNode( struct ListenSession *p_listen_session , char *domain , int domain_len )
{
	int			index ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct VirtualHost	*p ;
	
	index = CalcHash(domain,domain_len) % (p_listen_session->virtualhost_hashsize) ;
	p_hlist_head = p_listen_session->virtualhost_hash + index ;
	hlist_for_each_entry( p , p_hlist_head , struct VirtualHost , virtualhost_node )
	{
		if( p->domain_len == domain_len && STRNCMP( p->domain , == , domain , domain_len ) )
			return p;
	}
	
	return NULL;
}

