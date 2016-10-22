/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int InitMimeTypeHash( struct HetaoEnv *p_env , hetao_conf *p_config )
{
	int			i ;
	
	struct MimeType		*p_mime_type = NULL ;
	char			*p_type = NULL ;
	
	int			nret = 0 ;
	
	p_env->mime_type_hashsize = p_config->mime_types._mime_type_count * 2 ;
	p_env->mime_type_hash = (struct hlist_head *)malloc( sizeof(struct hlist_head) * p_env->mime_type_hashsize ) ;
	if( p_env->mime_type_hash == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , ERRNO );
		return -1;
	}
	memset( p_env->mime_type_hash , 0x00 , sizeof(struct hlist_head) * p_env->mime_type_hashsize );
	
	for( i = 0 ; i < p_env->mime_type_hashsize ; i++ )
	{
		INIT_HLIST_HEAD( p_env->mime_type_hash+i );
	}
	
	for( i = 0 ; i < p_config->mime_types._mime_type_count ; i++ )
	{
		p_type = strtok( p_config->mime_types.mime_type[i].type , " \r" ) ;
		while( p_type )
		{
			p_mime_type = (struct MimeType *)malloc( sizeof(struct MimeType) ) ;
			if( p_mime_type == NULL )
			{
				ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , ERRNO );
				return -1;
			}
			memset( p_mime_type , 0x00 , sizeof(struct MimeType) );
			
			strncpy( p_mime_type->type , p_type , sizeof(p_mime_type->type)-1 );
			p_mime_type->type_len = strlen(p_mime_type->type) ;
			strncpy( p_mime_type->mime , p_config->mime_types.mime_type[i].mime , sizeof(p_mime_type->mime)-1 );
			p_mime_type->compress_enable = p_config->mime_types.mime_type[i].compress_enable ;
			
			nret = PushMimeTypeHashNode( p_env , p_mime_type ) ;
			if( nret )
			{
				ErrorLog( __FILE__ , __LINE__ , "PushMimeTypeHashNode failed[%d]" , nret );
				return -1;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "add type[%s] mime[%s] ok" , p_mime_type->type , p_mime_type->mime );
			}
			
			p_type = strtok( NULL , " \t" ) ;
		}
	}
	
	return 0;
}

void CleanMimeTypeHash( struct HetaoEnv *p_env )
{
	int			i ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct hlist_node	*curr = NULL , *next = NULL ;
	struct MimeType		*p_mime_type = NULL ;
	
	for( i = 0 , p_hlist_head = p_env->mime_type_hash ; i < p_env->mime_type_hashsize ; i++ , p_hlist_head++ )
	{
		hlist_for_each_safe( curr , next , p_hlist_head )
		{
			hlist_del( curr );
			p_mime_type = container_of(curr,struct MimeType,mime_type_node) ;
			free( p_mime_type );
		}
	}
	
	free( p_env->mime_type_hash );
	
	return;
}

int PushMimeTypeHashNode( struct HetaoEnv *p_env , struct MimeType *p_mime_type )
{
	int			index ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct MimeType		*p ;
	
	index = CalcHash(p_mime_type->type,p_mime_type->type_len) % (p_env->mime_type_hashsize) ;
	p_hlist_head = p_env->mime_type_hash + index ;
	hlist_for_each_entry( p , p_hlist_head , struct MimeType , mime_type_node )
	{
		if( STRCMP( p->type , == , p_mime_type->type ) )
			return 1;
	}
	hlist_add_head( & (p_mime_type->mime_type_node) , p_hlist_head );
	
	return 0;
}

struct MimeType *QueryMimeTypeHashNode( struct HetaoEnv *p_env , char *type , int type_len )
{
	int			index ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct MimeType		*p ;
	
	index = CalcHash(type,type_len) % (p_env->mime_type_hashsize) ;
	p_hlist_head = p_env->mime_type_hash + index ;
	hlist_for_each_entry( p , p_hlist_head , struct MimeType , mime_type_node )
	{
		if( p->type_len == type_len && STRNCMP( p->type , == , type , type_len ) )
			return p;
	}
	
	return NULL;
}

