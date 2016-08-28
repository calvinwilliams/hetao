/*
 * hetao - High Performance Web Server
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "hetao_in.h"

int InitMimeTypeHash( struct HetaoServer *p_server )
{
	int			i ;
	struct hlist_head	*p_hlist_head = NULL ;
	
	p_server->mimetype_hashsize = p_server->p_config->mime_types._mime_type_count * 2 ;
	p_server->mimetype_hash = (struct hlist_head *)malloc( sizeof(struct hlist_head) * p_server->mimetype_hashsize ) ;
	if( p_server->mimetype_hash == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , errno );
		return -1;
	}
	memset( p_server->mimetype_hash , 0x00 , sizeof(struct hlist_head) * p_server->mimetype_hashsize );
	
	for( i = 0 , p_hlist_head = p_server->mimetype_hash ; i < p_server->mimetype_hashsize ; i++ , p_hlist_head++ )
	{
		INIT_HLIST_HEAD( p_hlist_head );
	}
	
	
	return 0;
}

void CleanMimeTypeHash( struct HetaoServer *p_server )
{
	int			i ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct hlist_node	*curr = NULL , *next = NULL ;
	struct MimeType		*p_mimetype = NULL ;
	
	for( i = 0 , p_hlist_head = p_server->mimetype_hash ; i < p_server->mimetype_hashsize ; i++ , p_hlist_head++ )
	{
		hlist_for_each_safe( curr , next , p_hlist_head )
		{
			hlist_del( curr );
			p_mimetype = container_of(curr,struct MimeType,mimetype_node) ;
			free( p_mimetype );
		}
	}
	
	free( p_server->mimetype_hash );
	
	return;
}

int PushMimeTypeHashNode( struct HetaoServer *p_server , struct MimeType *p_mimetype )
{
	int			index ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct MimeType		*p ;
	
	index = CalcHash(p_mimetype->type,p_mimetype->type_len) % (p_server->mimetype_hashsize) ;
	p_hlist_head = p_server->mimetype_hash + index ;
	hlist_for_each_entry( p , p_hlist_head , mimetype_node )
	{
		if( STRCMP( p->type , == , p_mimetype->type ) )
			return 1;
	}
	hlist_add_head( & (p_mimetype->mimetype_node) , p_hlist_head );
	
	return 0;
}

struct MimeType *QueryMimeTypeHashNode( struct HetaoServer *p_server , char *type , int type_len )
{
	int			index ;
	struct hlist_head	*p_hlist_head = NULL ;
	struct MimeType		*p ;
	
	index = CalcHash(type,type_len) % (p_server->mimetype_hashsize) ;
	p_hlist_head = p_server->mimetype_hash + index ;
	hlist_for_each_entry( p , p_hlist_head , mimetype_node )
	{
		if( p->type_len == type_len && STRNCMP( p->type , == , type , type_len ) )
			return p;
	}
	
	return NULL;
}

