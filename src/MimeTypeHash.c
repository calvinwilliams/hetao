/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int InitMimeTypeHash( struct HetaoEnv *p_env )
{
	int			i ;
	
	struct MimeType		*p_mimetype = NULL ;
	char			*p_type = NULL ;
	
	int			nret = 0 ;
	
	p_env->mimetype_hashsize = p_env->p_config->mime_types._mime_type_count * 2 ;
	p_env->mimetype_hash = (struct hlist_head *)malloc( sizeof(struct hlist_head) * p_env->mimetype_hashsize ) ;
	if( p_env->mimetype_hash == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , errno );
		return -1;
	}
	memset( p_env->mimetype_hash , 0x00 , sizeof(struct hlist_head) * p_env->mimetype_hashsize );
	
	for( i = 0 ; i < p_env->mimetype_hashsize ; i++ )
	{
		INIT_HLIST_HEAD( p_env->mimetype_hash+i );
	}
	
	for( i = 0 ; i < p_env->p_config->mime_types._mime_type_count ; i++ )
	{
		p_type = strtok( p_env->p_config->mime_types.mime_type[i].type , " \r" ) ;
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
			strncpy( p_mimetype->mime , p_env->p_config->mime_types.mime_type[i].mime , sizeof(p_mimetype->mime)-1 );
			p_mimetype->compress_enable = p_env->p_config->mime_types.mime_type[i].compress_enable ;
			
			nret = PushMimeTypeHashNode( p_env , p_mimetype ) ;
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
	
	return 0;
}

void CleanMimeTypeHash( struct HetaoEnv *p_env )
{
	int			i ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct hlist_node	*curr = NULL , *next = NULL ;
	struct MimeType		*p_mimetype = NULL ;
	
	for( i = 0 , p_hlist_head = p_env->mimetype_hash ; i < p_env->mimetype_hashsize ; i++ , p_hlist_head++ )
	{
		hlist_for_each_safe( curr , next , p_hlist_head )
		{
			hlist_del( curr );
			p_mimetype = container_of(curr,struct MimeType,mimetype_node) ;
			free( p_mimetype );
		}
	}
	
	free( p_env->mimetype_hash );
	
	return;
}

int PushMimeTypeHashNode( struct HetaoEnv *p_env , struct MimeType *p_mimetype )
{
	int			index ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct MimeType		*p ;
	
	index = CalcHash(p_mimetype->type,p_mimetype->type_len) % (p_env->mimetype_hashsize) ;
	p_hlist_head = p_env->mimetype_hash + index ;
	hlist_for_each_entry( p , p_hlist_head , mimetype_node )
	{
		if( STRCMP( p->type , == , p_mimetype->type ) )
			return 1;
	}
	hlist_add_head( & (p_mimetype->mimetype_node) , p_hlist_head );
	
	return 0;
}

struct MimeType *QueryMimeTypeHashNode( struct HetaoEnv *p_env , char *type , int type_len )
{
	int			index ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct MimeType		*p ;
	
	index = CalcHash(type,type_len) % (p_env->mimetype_hashsize) ;
	p_hlist_head = p_env->mimetype_hash + index ;
	hlist_for_each_entry( p , p_hlist_head , mimetype_node )
	{
		if( p->type_len == type_len && STRNCMP( p->type , == , type , type_len ) )
			return p;
	}
	
	return NULL;
}

