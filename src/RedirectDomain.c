/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int InitRedirectDomainHash( struct VirtualHost *p_virtual_host , int count )
{
	int			i ;
	struct hlist_head	*p_hlist_head = NULL ;
	
	p_virtual_host->redirect_domain_hashsize = count * 2 ;
	p_virtual_host->redirect_domain_hash = (struct hlist_head *)malloc( sizeof(struct hlist_head) * p_virtual_host->redirect_domain_hashsize ) ;
	if( p_virtual_host->redirect_domain_hash == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , ERRNO );
		return -1;
	}
	memset( p_virtual_host->redirect_domain_hash , 0x00 , sizeof(struct hlist_head) * p_virtual_host->redirect_domain_hashsize );
	
	for( i = 0 , p_hlist_head = p_virtual_host->redirect_domain_hash ; i < p_virtual_host->redirect_domain_hashsize ; i++ , p_hlist_head++ )
	{
		INIT_HLIST_HEAD( p_hlist_head );
	}
	
	return 0;
}

void CleanRedirectDomainHash( struct VirtualHost *p_virtual_host )
{
	int			i ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct hlist_node	*curr = NULL , *next = NULL ;
	struct RedirectDomain	*p_redirect_domain = NULL ;
	
	for( i = 0 , p_hlist_head = p_virtual_host->redirect_domain_hash ; i < p_virtual_host->redirect_domain_hashsize ; i++ , p_hlist_head++ )
	{
		hlist_for_each_safe( curr , next , p_hlist_head )
		{
			hlist_del( curr );
			p_redirect_domain = container_of(curr,struct RedirectDomain,redirect_domain_node) ;
			free( p_redirect_domain );
		}
	}
	
	free( p_virtual_host->redirect_domain_hash );
	p_virtual_host->redirect_domain_hash = NULL ;
	
	return;
}

int PushRedirectDomainHashNode( struct VirtualHost *p_virtual_host , struct RedirectDomain *p_redirect_domain )
{
	int			index ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct RedirectDomain	*p ;
	
	index = CalcHash(p_redirect_domain->domain,p_redirect_domain->domain_len) % (p_virtual_host->redirect_domain_hashsize) ;
	DebugLog( __FILE__ , __LINE__ , "RedirectDomain[%d]=CalcHash[%.*s][%d]" , index , p_redirect_domain->domain_len , p_redirect_domain->domain , p_virtual_host->redirect_domain_hashsize );
	p_hlist_head = p_virtual_host->redirect_domain_hash + index ;
	hlist_for_each_entry( p , p_hlist_head , struct RedirectDomain , redirect_domain_node )
	{
		if( STRCMP( p->domain , == , p_redirect_domain->domain ) )
			return 1;
	}
	hlist_add_head( & (p_redirect_domain->redirect_domain_node) , p_hlist_head );
	
	p_virtual_host->redirect_domain_count++;
	
	return 0;
}

struct RedirectDomain *QueryRedirectDomainHashNode( struct VirtualHost *p_virtual_host , char *domain , int domain_len )
{
	int			index ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct RedirectDomain	*p ;
	
	index = CalcHash(domain,domain_len) % (p_virtual_host->redirect_domain_hashsize) ;
	p_hlist_head = p_virtual_host->redirect_domain_hash + index ;
	hlist_for_each_entry( p , p_hlist_head , struct RedirectDomain , redirect_domain_node )
	{
		if( p->domain_len == domain_len && STRNCMP( p->domain , == , domain , domain_len ) )
			return p;
	}
	
	return NULL;
}

