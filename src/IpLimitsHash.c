/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

#define IP_LIMITS_HASHSIZE	(1<<16)

int InitIpLimitsHash( struct HetaoEnv *p_env )
{
	int			i ;
	struct hlist_head	*p_hlist_head = NULL ;
	
	p_env->iplimits_hash = (struct hlist_head *)malloc( sizeof(struct hlist_head) * IP_LIMITS_HASHSIZE ) ;
	if( p_env->iplimits_hash == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , errno );
		return -1;
	}
	memset( p_env->iplimits_hash , 0x00 , sizeof(struct hlist_head) * IP_LIMITS_HASHSIZE );
	
	for( i = 0 , p_hlist_head = p_env->iplimits_hash ; i < IP_LIMITS_HASHSIZE ; i++ , p_hlist_head++ )
	{
		INIT_HLIST_HEAD( p_hlist_head );
	}
	
	
	return 0;
}

void CleanIpLimitsHash( struct HetaoEnv *p_env )
{
	int			i ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct hlist_node	*curr = NULL , *next = NULL ;
	struct IpLimits		*p_iplimits = NULL ;
	
	for( i = 0 , p_hlist_head = p_env->iplimits_hash ; i < IP_LIMITS_HASHSIZE ; i++ , p_hlist_head++ )
	{
		hlist_for_each_safe( curr , next , p_hlist_head )
		{
			hlist_del( curr );
			p_iplimits = container_of(curr,struct IpLimits,iplimits_node) ;
			free( p_iplimits );
		}
	}
	
	free( p_env->iplimits_hash );
	p_env->iplimits_hash = NULL ;
	
	return;
}

int IncreaseIpLimitsHashNode( struct HetaoEnv *p_env , unsigned int ip )
{
	int			index ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct IpLimits		*p ;
	
	index = ip % IP_LIMITS_HASHSIZE ;
	DebugLog( __FILE__ , __LINE__ , "IpLimits[%d]=CalcHash[%u][%u]" , index , ip , IP_LIMITS_HASHSIZE );
	p_hlist_head = p_env->iplimits_hash + index ;
	hlist_for_each_entry( p , p_hlist_head , iplimits_node )
	{
		if( p->ip == ip )
		{
			DebugLog( __FILE__ , __LINE__ , "limits ip[%u] count[%d]->[%d]" , ip , p->count , p->count+1 );
			p->count++;
			if( p->count > p_env->limits__max_connections_per_ip )
			{
				DebugLog( __FILE__ , __LINE__ , "limits ip[%u] count[%d]->[%d]" , ip , p->count , p->count-1 );
				p->count--;
				return 1;
			}
			return 0;
		}
	}
	
	p = (struct IpLimits *)malloc( sizeof(struct IpLimits) ) ;
	if( p == NULL )
		return -1;
	p->ip = ip ;
	p->count = 1 ;
	hlist_add_head( & (p->iplimits_node) , p_hlist_head );
	DebugLog( __FILE__ , __LINE__ , "limits ip[%u] inserted" , ip );
	
	return 0;
}

int DecreaseIpLimitsHashNode( struct HetaoEnv *p_env , unsigned int ip )
{
	int			index ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct hlist_node	*p_curr = NULL , *p_next = NULL ;
	struct IpLimits		*p ;
	
	index = ip % IP_LIMITS_HASHSIZE ;
	DebugLog( __FILE__ , __LINE__ , "IpLimits[%d]=CalcHash[%u][%u]" , index , ip , IP_LIMITS_HASHSIZE );
	p_hlist_head = p_env->iplimits_hash + index ;
	hlist_for_each_safe( p_curr , p_next , p_hlist_head )
	{
		p = hlist_entry( p_curr , struct IpLimits , iplimits_node ) ;
		if( p->ip == ip )
		{
			DebugLog( __FILE__ , __LINE__ , "limits ip[%u] count[%d]->[%d]" , ip , p->count , p->count-1 );
			p->count--;
			if( p->count <= 0 )
			{
				DebugLog( __FILE__ , __LINE__ , "limits ip[%u] deleted" , ip );
				hlist_del( p_curr );
				free( p );
			}
			return 0;
		}
	}
	
	return 1;
}

