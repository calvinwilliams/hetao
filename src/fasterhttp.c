#include "fasterhttp.h"

int			__FASTERHTTP_VERSION_1_1_2 = 0 ;

struct HttpBuffer
{
	int			buf_size ;
	char			*base ;
	char			ref_flag ;
	char			*fill_ptr ;
	char			*process_ptr ;
} ;

struct HttpHeader
{
	struct HttpBuffer	*p_buffer ;
	int			name_offset ;
	int			name_len ;
	int			value_offset ;
	int			value_len ;
} ;

struct HttpHeaders
{
	struct HttpHeader	METHOD ;
	char			method ;
	struct HttpHeader	URI ;
	struct HttpHeader	VERSION ;
	char			version ;	/* 1.0 -> 10 */
						/* 1.1 -> 11 */
	
	struct HttpHeader	STATUSCODE ;
	struct HttpHeader	REASONPHRASE ;
	
	int			content_length ;
	
	char			transfer_encoding__chunked ;
	struct HttpHeader	TRAILER ;
	
	struct HttpHeader	CONNECTION ;
	char			connection__keepalive ;
	
	struct HttpHeader	*header_array ;
	int			header_array_size ;
	int			header_array_count ;
} ;

struct HttpEnv
{
	long			init_timeout ;
	struct timeval		timeout ;
	struct timeval		*p_timeout ;
	char			enable_response_compressing ;
	char			reforming_flag ;
	
	struct HttpBuffer	request_buffer ;
	struct HttpBuffer	response_buffer ;
	struct HttpBuffer	*p_append_buffer ;
	
	int			parse_step ;
	struct HttpHeaders	headers ;
	
	char			*body ;
	
	char			*chunked_body ;
	char			*chunked_body_end ;
	int			chunked_length ;
	int			chunked_length_length ;
	
	int			i ;
	void			*ptr ;
} ;

struct HttpStatus
{
	int	STATUS ;
	char	*STATUS_S ;
	char	*STATUS_T ;
} ;

char			g_init_httpstatus = 0 ;
struct HttpStatus	g_httpstatus_array[ 505 + 1 ] = { { 0 , NULL , NULL } } ;

void _DumpHexBuffer( FILE *fp , char *buf , long buflen )
{
	int		row_offset , col_offset ;
	
	fprintf( fp , "             0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F    0123456789ABCDEF\n" );
	
	row_offset = 0 ;
	col_offset = 0 ;
	while(1)
	{
		fprintf( fp , "0x%08X   " , row_offset * 16 );
		for( col_offset = 0 ; col_offset < 16 ; col_offset++ )
		{
			if( row_offset * 16 + col_offset < buflen )
			{
				fprintf( fp , "%02X " , *((unsigned char *)buf+row_offset*16+col_offset));
			}
			else
			{
				fprintf( fp , "   " );
			}
		}
		fprintf( fp , "  " );
		for( col_offset = 0 ; col_offset < 16 ; col_offset++ )
		{
			if( row_offset * 16 + col_offset < buflen )
			{
				if( isprint( (int)(unsigned char)*(buf+row_offset*16+col_offset) ) )
				{
					fprintf( fp , "%c" , *((unsigned char *)buf+row_offset*16+col_offset) );
				}
				else
				{
					fprintf( fp , "." );
				}
			}
			else
			{
				fprintf( fp , " " );
			}
		}
		fprintf( fp , "\n" );
		if( row_offset * 16 + col_offset >= buflen )
			break;
		row_offset++;
	}
	
	return;
}

struct HttpEnv *CreateHttpEnv()
{
	struct HttpEnv	*e = NULL ;
	
	int		nret = 0 ;
	
	ResetAllHttpStatus();
	
	e = (struct HttpEnv *)malloc( sizeof(struct HttpEnv) ) ;
	if( e == NULL )
	{
		DestroyHttpEnv( e );
		return NULL;
	}
	memset( e , 0x00 , sizeof(struct HttpEnv) );
	
	SetHttpTimeout( e , FASTERHTTP_TIMEOUT_DEFAULT );
	
	nret = InitHttpBuffer( & (e->request_buffer) , FASTERHTTP_REQUEST_BUFSIZE_DEFAULT ) ;
	if( nret )
	{
		DestroyHttpEnv( e );
		return NULL;
	}
	
	nret = InitHttpBuffer( & (e->response_buffer) , FASTERHTTP_RESPONSE_BUFSIZE_DEFAULT ) ;
	if( nret )
	{
		DestroyHttpEnv( e );
		return NULL;
	}
	
	e->headers.header_array_size = FASTERHTTP_HEADER_ARRAYSIZE_DEFAULT ;
	e->headers.header_array = (struct HttpHeader *)malloc( sizeof(struct HttpHeader) * e->headers.header_array_size ) ;
	if( e->headers.header_array == NULL )
	{
		DestroyHttpEnv( e );
		return NULL;
	}
	memset( e->headers.header_array , 0x00 , sizeof(struct HttpHeader) * e->headers.header_array_size );
	e->headers.header_array_count = 0 ;
	
	e->parse_step = FASTERHTTP_PARSESTEP_BEGIN ;
	
	return e;
}

static void ResetHttpTimeout( struct HttpEnv *e );

void ResetHttpEnv( struct HttpEnv *e )
{
	struct HttpHeaders	*p_headers = & (e->headers) ;
	struct HttpBuffer	*b = NULL ;
	
	if( e == NULL )
		return;
	
	/* struct HttpEnv */
	
	ResetHttpTimeout( e );
	
	if( e->enable_response_compressing == 2 )
		e->enable_response_compressing = 1 ;
	
	b = & (e->request_buffer) ;
	if( b->ref_flag == 0 && UNLIKELY( b->process_ptr < b->fill_ptr ) && e->reforming_flag == 1 )
	{
		ReformingHttpBuffer( b );
		e->reforming_flag = 0 ;
	}
	else
	{
		ResetHttpBuffer( b );
	}
	if( UNLIKELY( b->ref_flag == 0 && b->buf_size > FASTERHTTP_REQUEST_BUFSIZE_MAX && b->fill_ptr-b->process_ptr<FASTERHTTP_REQUEST_BUFSIZE_MAX ) )
		ReallocHttpBuffer( b , FASTERHTTP_REQUEST_BUFSIZE_MAX );
	
	b = & (e->response_buffer) ;
	if( b->ref_flag == 0 && UNLIKELY( b->process_ptr < b->fill_ptr ) && e->reforming_flag == 1 )
	{
		ReformingHttpBuffer( b );
		e->reforming_flag = 0 ;
	}
	else
	{
		ResetHttpBuffer( b );
	}
	if( UNLIKELY( b->ref_flag == 0 && b->buf_size > FASTERHTTP_RESPONSE_BUFSIZE_MAX ) )
		ReallocHttpBuffer( b , FASTERHTTP_RESPONSE_BUFSIZE_MAX );
	
	e->p_append_buffer = NULL ;
	
	e->parse_step = FASTERHTTP_PARSESTEP_BEGIN ;
	
	e->body = NULL ;
	
	e->chunked_body = NULL ;
	e->chunked_body_end = NULL ;
	e->chunked_length = 0 ;
	e->chunked_length_length = 0 ;
	
	/* struct HttpHeaders */
	
	p_headers->METHOD.p_buffer = NULL ;
	p_headers->METHOD.value_offset = 0 ;
	p_headers->METHOD.value_len = 0 ;
	p_headers->method = 0 ;
	p_headers->URI.p_buffer = NULL ;
	p_headers->URI.value_offset = 0 ;
	p_headers->URI.value_len = 0 ;
	p_headers->VERSION.p_buffer = NULL ;
	p_headers->VERSION.value_offset = 0 ;
	p_headers->VERSION.value_len = 0 ;
	// p_headers->version = 0 ;
	p_headers->STATUSCODE.p_buffer = NULL ;
	p_headers->STATUSCODE.value_offset = 0 ;
	p_headers->STATUSCODE.value_len = 0 ;
	p_headers->REASONPHRASE.p_buffer = NULL ;
	p_headers->REASONPHRASE.value_offset = 0 ;
	p_headers->REASONPHRASE.value_len = 0 ;
	p_headers->content_length = 0 ;
	p_headers->TRAILER.p_buffer = NULL ;
	p_headers->TRAILER.value_offset = 0 ;
	p_headers->TRAILER.value_len = 0 ;
	p_headers->transfer_encoding__chunked = 0 ;
	// p_headers->connection__keepalive = 0 ;
	
	if( UNLIKELY( p_headers->header_array_size > FASTERHTTP_HEADER_ARRAYSIZE_MAX ) )
	{
		struct HttpHeader	*p = NULL ;
		p = (struct HttpHeader *)realloc( p_headers->header_array , sizeof(struct HttpHeader) * FASTERHTTP_HEADER_ARRAYSIZE_DEFAULT ) ;
		if( p )
		{
			memset( p , 0x00 , sizeof(struct HttpHeader) * FASTERHTTP_HEADER_ARRAYSIZE_DEFAULT );
			p_headers->header_array = p ;
			p_headers->header_array_size = FASTERHTTP_HEADER_ARRAYSIZE_DEFAULT ;
		}
	}
	
	memset( p_headers->header_array , 0x00 , sizeof(struct HttpHeader) * p_headers->header_array_size );
	p_headers->header_array_count = 0 ;
	
	return;
}

void DestroyHttpEnv( struct HttpEnv *e )
{
	if( e )
	{
		CleanHttpBuffer( & (e->request_buffer) );
		CleanHttpBuffer( & (e->response_buffer) );
		
		if( e->headers.header_array )
			free( e->headers.header_array );
		
		free( e );
	}
	
	return;
}

static void ResetHttpTimeout( struct HttpEnv *e )
{
	if( e->init_timeout >= 0 )
	{
		e->timeout.tv_sec = e->init_timeout ;
		e->timeout.tv_usec = 0 ;
		e->p_timeout = & (e->timeout) ;
	}
	else
	{
		e->p_timeout = NULL ;
	}
	
	return;
}

void SetHttpTimeout( struct HttpEnv *e , long timeout )
{
	e->init_timeout = timeout ;
	
	ResetHttpTimeout( e );
	
	return;
}

struct timeval *GetHttpElapse( struct HttpEnv *e )
{
	return e->p_timeout;
}

void EnableHttpResponseCompressing( struct HttpEnv *e , int enable_response_compressing )
{
	e->enable_response_compressing = (char)enable_response_compressing ;
	return;
}

void SetParserCustomIntData( struct HttpEnv *e , int i )
{
	e->i = i ;
	return;
}

int GetParserCustomIntData( struct HttpEnv *e )
{
	return e->i;
}

void SetParserCustomPtrData( struct HttpEnv *e , void *ptr )
{
	e->ptr = ptr ;
	return;
}

void *GetParserCustomPtrData( struct HttpEnv *e )
{
	return e->ptr;
}

void ResetAllHttpStatus()
{
	if( g_init_httpstatus == 0 )
	{
		SetHttpStatus( HTTP_CONTINUE , HTTP_CONTINUE_S , HTTP_CONTINUE_T );
		SetHttpStatus( HTTP_SWITCHING_PROTOCOL , HTTP_SWITCHING_PROTOCOL_S , HTTP_SWITCHING_PROTOCOL_T );
		SetHttpStatus( HTTP_OK , HTTP_OK_S , HTTP_OK_T );
		SetHttpStatus( HTTP_CREATED , HTTP_CREATED_S , HTTP_CREATED_T );
		SetHttpStatus( HTTP_ACCEPTED , HTTP_ACCEPTED_S , HTTP_ACCEPTED_T );
		SetHttpStatus( HTTP_NON_AUTHORITATIVE_INFORMATION , HTTP_NON_AUTHORITATIVE_INFORMATION_S , HTTP_NON_AUTHORITATIVE_INFORMATION_T );
		SetHttpStatus( HTTP_NO_CONTENT , HTTP_NO_CONTENT_S , HTTP_NO_CONTENT_T );
		SetHttpStatus( HTTP_RESET_CONTENT , HTTP_RESET_CONTENT_S , HTTP_RESET_CONTENT_T );
		SetHttpStatus( HTTP_PARTIAL_CONTENT , HTTP_PARTIAL_CONTENT_S , HTTP_PARTIAL_CONTENT_T );
		SetHttpStatus( HTTP_MULTIPLE_CHOICES , HTTP_MULTIPLE_CHOICES_S , HTTP_MULTIPLE_CHOICES_T );
		SetHttpStatus( HTTP_MOVED_PERMANNETLY , HTTP_MOVED_PERMANNETLY_S , HTTP_MOVED_PERMANNETLY_T );
		SetHttpStatus( HTTP_FOUND , HTTP_FOUND_S , HTTP_FOUND_T );
		SetHttpStatus( HTTP_SEE_OTHER , HTTP_SEE_OTHER_S , HTTP_SEE_OTHER_T );
		SetHttpStatus( HTTP_NOT_MODIFIED , HTTP_NOT_MODIFIED_S , HTTP_NOT_MODIFIED_T );
		SetHttpStatus( HTTP_USE_PROXY , HTTP_USE_PROXY_S , HTTP_USE_PROXY_T );
		SetHttpStatus( HTTP_TEMPORARY_REDIRECT , HTTP_TEMPORARY_REDIRECT_S , HTTP_TEMPORARY_REDIRECT_T );
		SetHttpStatus( HTTP_BAD_REQUEST , HTTP_BAD_REQUEST_S , HTTP_BAD_REQUEST_T );
		SetHttpStatus( HTTP_UNAUTHORIZED , HTTP_UNAUTHORIZED_S , HTTP_UNAUTHORIZED_T );
		SetHttpStatus( HTTP_PAYMENT_REQUIRED , HTTP_PAYMENT_REQUIRED_S , HTTP_PAYMENT_REQUIRED_T );
		SetHttpStatus( HTTP_FORBIDDEN , HTTP_FORBIDDEN_S , HTTP_FORBIDDEN_T );
		SetHttpStatus( HTTP_NOT_FOUND , HTTP_NOT_FOUND_S , HTTP_NOT_FOUND_T );
		SetHttpStatus( HTTP_METHOD_NOT_ALLOWED , HTTP_METHOD_NOT_ALLOWED_S , HTTP_METHOD_NOT_ALLOWED_T );
		SetHttpStatus( HTTP_NOT_ACCEPTABLE , HTTP_NOT_ACCEPTABLE_S , HTTP_NOT_ACCEPTABLE_T );
		SetHttpStatus( HTTP_PROXY_AUTHENTICATION_REQUIRED , HTTP_PROXY_AUTHENTICATION_REQUIRED_S , HTTP_PROXY_AUTHENTICATION_REQUIRED_T );
		SetHttpStatus( HTTP_REQUEST_TIMEOUT , HTTP_REQUEST_TIMEOUT_S , HTTP_REQUEST_TIMEOUT_T );
		SetHttpStatus( HTTP_CONFLICT , HTTP_CONFLICT_S , HTTP_CONFLICT_T );
		SetHttpStatus( HTTP_GONE , HTTP_GONE_S , HTTP_GONE_T );
		SetHttpStatus( HTTP_LENGTH_REQUIRED , HTTP_LENGTH_REQUIRED_S , HTTP_LENGTH_REQUIRED_T );
		SetHttpStatus( HTTP_PRECONDITION_FAILED , HTTP_PRECONDITION_FAILED_S , HTTP_PRECONDITION_FAILED_T );
		SetHttpStatus( HTTP_REQUEST_ENTITY_TOO_LARGE , HTTP_REQUEST_ENTITY_TOO_LARGE_S , HTTP_REQUEST_ENTITY_TOO_LARGE_T );
		SetHttpStatus( HTTP_URI_TOO_LONG , HTTP_URI_TOO_LONG_S , HTTP_URI_TOO_LONG_T );
		SetHttpStatus( HTTP_UNSUPPORTED_MEDIA_TYPE , HTTP_UNSUPPORTED_MEDIA_TYPE_S , HTTP_UNSUPPORTED_MEDIA_TYPE_T );
		SetHttpStatus( HTTP_REQUESTED_RANGE_NOT_SATISFIABLE , HTTP_REQUESTED_RANGE_NOT_SATISFIABLE_S , HTTP_REQUESTED_RANGE_NOT_SATISFIABLE_T );
		SetHttpStatus( HTTP_EXPECTATION_FAILED , HTTP_EXPECTATION_FAILED_S , HTTP_EXPECTATION_FAILED_T );
		SetHttpStatus( HTTP_INTERNAL_SERVER_ERROR , HTTP_INTERNAL_SERVER_ERROR_S , HTTP_INTERNAL_SERVER_ERROR_T );
		SetHttpStatus( HTTP_NOT_IMPLEMENTED , HTTP_NOT_IMPLEMENTED_S , HTTP_NOT_IMPLEMENTED_T );
		SetHttpStatus( HTTP_BAD_GATEWAY , HTTP_BAD_GATEWAY_S , HTTP_BAD_GATEWAY_T );
		SetHttpStatus( HTTP_SERVICE_UNAVAILABLE , HTTP_SERVICE_UNAVAILABLE_S , HTTP_SERVICE_UNAVAILABLE_T );
		SetHttpStatus( HTTP_GATEWAY_TIMEOUT , HTTP_GATEWAY_TIMEOUT_S , HTTP_GATEWAY_TIMEOUT_T );
		SetHttpStatus( HTTP_VERSION_NOT_SUPPORTED , HTTP_VERSION_NOT_SUPPORTED_S , HTTP_VERSION_NOT_SUPPORTED_T );
		
		g_init_httpstatus = 1 ;
	}
	
	return;
}

void SetHttpStatus( int status_code , char *status_code_s , char *status_text )
{
	g_httpstatus_array[status_code].STATUS = status_code ;
	g_httpstatus_array[status_code].STATUS_S = status_code_s ;
	g_httpstatus_array[status_code].STATUS_T = status_text ;
	return;
}

void GetHttpStatus( int status_code , char **pp_status_code_s , char **pp_status_text )
{
	if( pp_status_code_s )
		(*pp_status_code_s) = g_httpstatus_array[status_code].STATUS_S ;
	if( pp_status_text )
		(*pp_status_text) = g_httpstatus_array[status_code].STATUS_T ;
	return;
}

#define DEBUG_COMM		0

static void AjustTimeval( struct timeval *ptv , struct timeval *t1 , struct timeval *t2 )
{
#if ( defined _WIN32 )
	ptv->tv_sec -= ( t2->tv_sec - t1->tv_sec ) ;
#elif ( defined __unix ) || ( defined __linux__ )
	ptv->tv_sec -= ( t2->tv_sec - t1->tv_sec ) ;
	ptv->tv_usec -= ( t2->tv_usec - t1->tv_usec ) ;
	while( ptv->tv_usec < 0 )
	{
		ptv->tv_usec += 1000000 ;
		ptv->tv_sec--;
	}
#endif
	if( ptv->tv_sec < 0 )
		ptv->tv_sec = 0 ;
	
	return;
}

static int SendHttpBuffer( SOCKET sock , SSL *ssl , struct HttpEnv *e , struct HttpBuffer *b , int wait_flag )
{
	struct timeval	t1 , t2 ;
	fd_set		write_fds ;
	int		len ;
	int		nret = 0 ;
	
	if( b->process_ptr >= b->fill_ptr )
		return 0;
	
	if( e->init_timeout >= 0 )
	{
#if ( defined _WIN32 )
		t1.tv_sec = (long)time( NULL ) ;
#elif ( defined __unix ) || ( defined __linux__ )
		gettimeofday( & t1 , NULL );
#endif
	}
	
	if( wait_flag )
	{
		FD_ZERO( & write_fds );
		FD_SET( sock , & write_fds );
		nret = select( sock+1 , NULL , & write_fds , NULL , e->p_timeout ) ;
		if( nret == 0 )
			return FASTERHTTP_ERROR_TCP_SELECT_SEND_TIMEOUT;
		else if( nret != 1 )
			return FASTERHTTP_ERROR_TCP_SELECT_SEND;
	}
	
	if( ssl == NULL )
		len = (int)send( sock , b->process_ptr , b->fill_ptr-b->process_ptr , 0 ) ;
	else
		len = (int)SSL_write( ssl , b->process_ptr , b->fill_ptr-b->process_ptr ) ;
	if( len == -1 )
	{
		if( ERRNO == EAGAIN || ERRNO == EWOULDBLOCK )
			return FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK;
		return FASTERHTTP_ERROR_TCP_SEND;
	}
	
#if DEBUG_COMM
printf( "send\n" );
_DumpHexBuffer( stdout , b->process_ptr , len );
#endif

	if( e->init_timeout >= 0 )
	{
#if ( defined _WIN32 )
		t2.tv_sec = (long)time( NULL ) ;
#elif ( defined __unix ) || ( defined __linux__ )
		gettimeofday( & t2 , NULL );
#endif
		AjustTimeval( e->p_timeout , & t1 , & t2 );
	}
	
	b->process_ptr += len ;
	
	if( b->process_ptr >= b->fill_ptr )
		return 0;
	else
		return FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK;
}

static int SendHttpBuffer1( SOCKET sock , SSL *ssl , struct HttpEnv *e , struct HttpBuffer *b , int wait_flag )
{
	struct timeval	t1 , t2 ;
	fd_set		write_fds ;
	int		len ;
	int		nret = 0 ;
	
	if( b->process_ptr >= b->fill_ptr )
		return 0;
	
	if( e->init_timeout >= 0 )
	{
#if ( defined _WIN32 )
		t1.tv_sec = (long)time( NULL ) ;
#elif ( defined __unix ) || ( defined __linux__ )
		gettimeofday( & t1 , NULL );
#endif
	}
	
	if( wait_flag )
	{
		FD_ZERO( & write_fds );
		FD_SET( sock , & write_fds );
		nret = select( sock+1 , NULL , & write_fds , NULL , e->p_timeout ) ;
		if( nret == 0 )
			return FASTERHTTP_ERROR_TCP_SELECT_SEND_TIMEOUT;
		else if( nret != 1 )
			return FASTERHTTP_ERROR_TCP_SELECT_SEND;
	}
	
	if( ssl == NULL )
		len = (int)send( sock , b->process_ptr , 10 , 0 ) ;
	else
		len = (int)SSL_write( ssl , b->process_ptr , 10 ) ;
	if( len == -1 )
	{
		if( ERRNO == EAGAIN || ERRNO == EWOULDBLOCK )
			return FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK;
		return FASTERHTTP_ERROR_TCP_SEND;
	}
	
#if DEBUG_COMM
printf( "send\n" );
_DumpHexBuffer( stdout , b->process_ptr , len );
#endif

	if( e->init_timeout >= 0 )
	{
#if ( defined _WIN32 )
		t2.tv_sec = (long)time( NULL ) ;
#elif ( defined __unix ) || ( defined __linux__ )
		gettimeofday( & t2 , NULL );
#endif
		AjustTimeval( e->p_timeout , & t1 , & t2 );
	}
	
	b->process_ptr += len ;
	
	if( b->process_ptr >= b->fill_ptr )
		return 0;
	else
		return FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK;
}

static int ReceiveHttpBuffer( SOCKET sock , SSL *ssl , struct HttpEnv *e , struct HttpBuffer *b , int wait_flag )
{
	struct timeval	t1 , t2 ;
	fd_set		read_fds ;
	int		len ;
	int		nret = 0 ;
	
	if( b->process_ptr == b->base && b->process_ptr < b->fill_ptr )
		return 0;
	
	while( (b->fill_ptr-b->base) >= b->buf_size-1 )
	{
		nret = ReallocHttpBuffer( b , -1 ) ;
		if( nret )
			return nret;
	}
	
	if( e->init_timeout >= 0 )
	{
#if ( defined _WIN32 )
		t1.tv_sec = (long)time( NULL ) ;
#elif ( defined __unix ) || ( defined __linux__ )
		gettimeofday( & t1 , NULL );
#endif
	}
	
	if( wait_flag )
	{
		FD_ZERO( & read_fds );
		FD_SET( sock , & read_fds );
		nret = select( sock+1 , & read_fds , NULL , NULL , e->p_timeout ) ;
		if( nret == 0 )
			return FASTERHTTP_ERROR_TCP_SELECT_RECEIVE_TIMEOUT;
		else if( nret != 1 )
			return FASTERHTTP_ERROR_TCP_SELECT_RECEIVE;
	}
	
	if( ssl == NULL )
		len = (int)recv( sock , b->fill_ptr , b->buf_size-1 - (b->fill_ptr-b->base) , 0 ) ;
	else
		len = (int)SSL_read( ssl , b->fill_ptr , b->buf_size-1 - (b->fill_ptr-b->base) ) ;
	if( len == -1 )
	{
		if( errno == EAGAIN || errno == EWOULDBLOCK )
			return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
		else if( errno == ECONNRESET )
			return FASTERHTTP_ERROR_TCP_CLOSE;
		return FASTERHTTP_ERROR_TCP_RECEIVE;
	}
	else if( len == 0 )
	{
		return FASTERHTTP_ERROR_TCP_CLOSE;
	}
	
#if DEBUG_COMM
printf( "recv\n" );
_DumpHexBuffer( stdout , b->fill_ptr , len );
#endif

	if( e->init_timeout >= 0 )
	{
#if ( defined _WIN32 )
		t2.tv_sec = (long)time( NULL ) ;
#elif ( defined __unix ) || ( defined __linux__ )
		gettimeofday( & t2 , NULL );
#endif
		AjustTimeval( e->p_timeout , & t1 , & t2 );
	}
	
	b->fill_ptr += len ;
	
	return 0;
}

static int ReceiveHttpBuffer1( SOCKET sock , SSL *ssl , struct HttpEnv *e , struct HttpBuffer *b , int wait_flag )
{
	struct timeval	t1 , t2 ;
	fd_set		read_fds ;
	int		len ;
	int		nret = 0 ;
	
	if( b->process_ptr == b->base && b->process_ptr < b->fill_ptr )
		return 0;
	
	while( (b->fill_ptr-b->base) >= b->buf_size-1 )
	{
		nret = ReallocHttpBuffer( b , -1 ) ;
		if( nret )
			return nret;
	}
	
	if( e->init_timeout >= 0 )
	{
#if ( defined _WIN32 )
		t1.tv_sec = (long)time( NULL ) ;
#elif ( defined __unix ) || ( defined __linux__ )
		gettimeofday( & t1 , NULL );
#endif
	}
	
	if( wait_flag )
	{
		FD_ZERO( & read_fds );
		FD_SET( sock , & read_fds );
		nret = select( sock+1 , & read_fds , NULL , NULL , e->p_timeout ) ;
		if( nret == 0 )
			return FASTERHTTP_ERROR_TCP_SELECT_RECEIVE_TIMEOUT;
		else if( nret != 1 )
			return FASTERHTTP_ERROR_TCP_SELECT_RECEIVE;
	}
	
	if( ssl == NULL )
		len = (long)recv( sock , b->fill_ptr , 1 , 0 ) ;
	else
		len = (long)SSL_read( ssl , b->fill_ptr , 1 ) ;
	if( len == -1 )
	{
		if( errno == EWOULDBLOCK )
			return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
		else if( errno == ECONNRESET )
			return FASTERHTTP_ERROR_TCP_CLOSE;
		return FASTERHTTP_ERROR_TCP_RECEIVE;
	}
	else if( len == 0 )
	{
		return FASTERHTTP_ERROR_TCP_CLOSE;
	}
	
	if( e->init_timeout >= 0 )
	{
#if ( defined _WIN32 )
		t2.tv_sec = (long)time( NULL ) ;
#elif ( defined __unix ) || ( defined __linux__ )
		gettimeofday( & t2 , NULL );
#endif
		AjustTimeval( e->p_timeout , & t1 , & t2 );
	}
	
	b->fill_ptr += len ;
	
	return 0;
}

#define DEBUG_PARSER		0

static int ParseHttpBuffer( struct HttpEnv *e , struct HttpBuffer *b )
{
	register char		*p = b->process_ptr ;
	char			*p2 = NULL ;
	
	int			*p_parse_step = & (e->parse_step) ;
	struct HttpHeader	*p_METHOD = & (e->headers.METHOD) ;
	struct HttpHeader	*p_URI = & (e->headers.URI) ;
	struct HttpHeader	*p_VERSION = & (e->headers.VERSION) ;
	struct HttpHeader	*p_STATUSCODE = & (e->headers.STATUSCODE) ;
	struct HttpHeader	*p_REASONPHRASE = & (e->headers.REASONPHRASE) ;
	struct HttpHeader	*p_TRAILER = & (e->headers.TRAILER) ;
	
	struct HttpHeader	*p_header = & (e->headers.header_array[e->headers.header_array_count]) ;
	char			*fill_ptr = b->fill_ptr ;
	
	char			*p_method = & (e->headers.method) ;
	char			*p_version = & (e->headers.version) ;
	int			*p_content_length = & (e->headers.content_length) ;
	char			*p_transfer_encoding__chunked = & (e->headers.transfer_encoding__chunked) ;
	char			*p_connection__keepalive = & (e->headers.connection__keepalive) ;
	
#if DEBUG_PARSER
	setbuf( stdout , NULL );
	printf( "DEBUG_PARSER >>>>>>>>> ParseHttpBuffer\n" );
	_DumpHexBuffer( stdout , b->process_ptr , (long)(b->fill_ptr-b->process_ptr) );
#endif
	if( UNLIKELY( *(p_parse_step) == FASTERHTTP_PARSESTEP_BEGIN ) )
	{
		if( b == & (e->request_buffer) )
		{
			*(p_parse_step) = FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_METHOD0 ;
		}
		else if( b == & (e->response_buffer) )
		{
			*(p_parse_step) = FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_VERSION0 ;
		}
		else
		{
			return FASTERHTTP_ERROR_PARAMTER;
		}
	}
	
	switch( *(p_parse_step) )
	{
		case FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_METHOD0 :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_METHOD0\n" );
#endif
			
			for( ; UNLIKELY( (*p) == ' ' && p < fill_ptr ) ; p++ )
				;
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			if( UNLIKELY( (*p) == HTTP_NEWLINE ) )
				return FASTERHTTP_ERROR_HTTP_HEADERSTARTLINE_INVALID;
			p_METHOD->p_buffer = b ;
			p_METHOD->value_offset = p - b->base ;
			p++;
			
			*(p_parse_step) = FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_METHOD ;
			
		case FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_METHOD :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_METHOD\n" );
#endif
			
			for( ; LIKELY( (*p) != ' ' && (*p) != HTTP_NEWLINE && p < fill_ptr ) ; p++ )
				;
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			if( UNLIKELY( (*p) == HTTP_NEWLINE ) )
				return FASTERHTTP_ERROR_HTTP_HEADERSTARTLINE_INVALID;
			p_METHOD->value_len = p - (b->base+p_METHOD->value_offset) ;
			p++;
			
			p2 = b->base+p_METHOD->value_offset ;
			if( LIKELY( p_METHOD->value_len == 3 ) )
			{
				if( LIKELY( *(p2) == HTTP_METHOD_GET[0] && *(p2+1) == HTTP_METHOD_GET[1] && *(p2+2) == HTTP_METHOD_GET[2] ) )
					(*p_method) = HTTP_METHOD_GET_N ;
				else if( *(p2) == HTTP_METHOD_PUT[0] && *(p2+1) == HTTP_METHOD_PUT[1] && *(p2+2) == HTTP_METHOD_PUT[2] )
					(*p_method) = HTTP_METHOD_PUT_N ;
				else
					return FASTERHTTP_ERROR_METHOD_INVALID;
			}
			else if( LIKELY( p_METHOD->value_len == 4 ) )
			{
				if( LIKELY( *(p2) == HTTP_METHOD_POST[0] && *(p2+1) == HTTP_METHOD_POST[1] && *(p2+2) == HTTP_METHOD_POST[2]
					&& *(p2+3) == HTTP_METHOD_POST[3] ) )
					(*p_method) = HTTP_METHOD_POST_N ;
				else if( *(p2) == HTTP_METHOD_HEAD[0] && *(p2+1) == HTTP_METHOD_HEAD[1] && *(p2+2) == HTTP_METHOD_HEAD[2]
					&& *(p2+3) == HTTP_METHOD_HEAD[3] )
					(*p_method) = HTTP_METHOD_HEAD_N ;
				else
					return FASTERHTTP_ERROR_METHOD_INVALID;
			}
			else if( p_METHOD->value_len == 5 )
			{
				if( *(p2) == HTTP_METHOD_TRACE[0] && *(p2+1) == HTTP_METHOD_TRACE[1] && *(p2+2) == HTTP_METHOD_TRACE[2]
					&& *(p2+3) == HTTP_METHOD_TRACE[3] && *(p2+4) == HTTP_METHOD_TRACE[4] )
					(*p_method) = HTTP_METHOD_TRACE_N ;
				else
					return FASTERHTTP_ERROR_METHOD_INVALID;
			}
			else if( p_METHOD->value_len == 6 )
			{
				if( *(p2) == HTTP_METHOD_DELETE[0] && *(p2+1) == HTTP_METHOD_DELETE[1] && *(p2+2) == HTTP_METHOD_DELETE[2]
					&& *(p2+3) == HTTP_METHOD_DELETE[3] && *(p2+4) == HTTP_METHOD_DELETE[4] && *(p2+5) == HTTP_METHOD_DELETE[5] )
					(*p_method) = HTTP_METHOD_DELETE_N ;
				else
					return FASTERHTTP_ERROR_METHOD_INVALID;
			}
			else if( p_METHOD->value_len == 7 )
			{
				if( *(p2) == HTTP_METHOD_OPTIONS[0] && *(p2+1) == HTTP_METHOD_OPTIONS[1] && *(p2+2) == HTTP_METHOD_OPTIONS[2]
					&& *(p2+3) == HTTP_METHOD_OPTIONS[3] && *(p2+4) == HTTP_METHOD_OPTIONS[4] && *(p2+5) == HTTP_METHOD_OPTIONS[5]
					&& *(p2+6) == HTTP_METHOD_OPTIONS[6] )
					(*p_method) = HTTP_METHOD_OPTIONS_N ;
				else
					return FASTERHTTP_ERROR_METHOD_INVALID;
			}
			else
			{
				return FASTERHTTP_ERROR_METHOD_INVALID;
			}
			
			*(p_parse_step) = FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_URI0 ;
			
		case FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_URI0 :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_URI0\n" );
#endif
			
			for( ; UNLIKELY( (*p) == ' ' && p < fill_ptr ) ; p++ )
				;
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			if( UNLIKELY( (*p) == HTTP_NEWLINE ) )
				return FASTERHTTP_ERROR_HTTP_HEADERSTARTLINE_INVALID;
			p_URI->p_buffer = b ;
			p_URI->value_offset = p - b->base ;
			p++;
			
			*(p_parse_step) = FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_URI ;
			
		case FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_URI :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_URI\n" );
#endif
			
			for( ; LIKELY( (*p) != ' ' && (*p) != HTTP_NEWLINE && p < fill_ptr ) ; p++ )
				;
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			if( UNLIKELY( (*p) == HTTP_NEWLINE ) )
				return FASTERHTTP_ERROR_HTTP_HEADERSTARTLINE_INVALID;
			p_URI->value_len = p - (b->base+p_URI->value_offset) ;
			p++;
			
			*(p_parse_step) = FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_VERSION0 ;
			
		case FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_VERSION0 :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_VERSION0\n" );
#endif
			
			for( ; UNLIKELY( (*p) == ' ' && p < fill_ptr ) ; p++ )
				;
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			if( UNLIKELY( (*p) == HTTP_NEWLINE ) )
				return FASTERHTTP_ERROR_HTTP_HEADERSTARTLINE_INVALID;
			p_VERSION->p_buffer = b ;
			p_VERSION->value_offset = p - b->base ;
			p++;
			
			*(p_parse_step) = FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_VERSION ;
			
		case FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_VERSION :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_VERSION\n" );
#endif
			
			for( ; LIKELY( (*p) != HTTP_NEWLINE && p < fill_ptr ) ; p++ )
				;
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			p_VERSION->value_len = p - (b->base+p_VERSION->value_offset) ;
			if( LIKELY( *(p-1) == HTTP_RETURN ) )
				p_VERSION->value_len--;
			p++;
			
			p2 = (b->base+p_VERSION->value_offset) ;
			if( LIKELY( p_VERSION->value_len == sizeof(HTTP_VERSION_1_0)-1 ) )
			{
				if( LIKELY( *(p2) == HTTP_VERSION_1_0[0] && *(p2+1) == HTTP_VERSION_1_0[1] && *(p2+2) == HTTP_VERSION_1_0[2]
					&& *(p2+3) == HTTP_VERSION_1_0[3] && *(p2+4) == HTTP_VERSION_1_0[4] && *(p2+5) == HTTP_VERSION_1_0[5]
					&& *(p2+6) == HTTP_VERSION_1_0[6] ) )
				{
					if( UNLIKELY( *(p2+7) == HTTP_VERSION_1_0[7] ) )
						*(p_version) = HTTP_VERSION_1_0_N ;
					else if( LIKELY( *(p2+7) == HTTP_VERSION_1_1[7] ) )
						*(p_version) = HTTP_VERSION_1_1_N ;
					else
						return FASTERHTTP_ERROR_VERSION_NOT_SUPPORTED;
				}
			}
			else
			{
				return FASTERHTTP_ERROR_VERSION_NOT_SUPPORTED;
			}
			
			*(p_parse_step) = FASTERHTTP_PARSESTEP_HEADER_NAME0 ;
			goto _GOTO_PARSESTEP_HEADER_NAME0;
			
		case FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_VERSION0 :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_VERSION0\n" );
#endif
			
			for( ; UNLIKELY( (*p) == ' ' && p < fill_ptr ) ; p++ )
				;
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			if( UNLIKELY( (*p) == HTTP_NEWLINE ) )
				return FASTERHTTP_ERROR_HTTP_HEADERSTARTLINE_INVALID;
			p_VERSION->p_buffer = b ;
			p_VERSION->value_offset = p - b->base ;
			p++;
			
			*(p_parse_step) = FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_VERSION ;
			
		case FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_VERSION :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_VERSION\n" );
#endif
			
			for( ; LIKELY( (*p) != ' ' && (*p) != HTTP_NEWLINE && p < fill_ptr ) ; p++ )
				;
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			if( UNLIKELY( (*p) == HTTP_NEWLINE ) )
				return FASTERHTTP_ERROR_HTTP_HEADERSTARTLINE_INVALID;
			p_VERSION->value_len = p - (b->base+p_VERSION->value_offset) ;
			p++;
			
			p2 = b->base+p_VERSION->value_offset ;
			if( LIKELY( p_VERSION->value_len == sizeof(HTTP_VERSION_1_0)-1 ) )
			{
				if( LIKELY( *(p2) == HTTP_VERSION_1_0[0] && *(p2+1) == HTTP_VERSION_1_0[1] && *(p2+2) == HTTP_VERSION_1_0[2]
					&& *(p2+3) == HTTP_VERSION_1_0[3] && *(p2+4) == HTTP_VERSION_1_0[4] && *(p2+5) == HTTP_VERSION_1_0[5]
					&& *(p2+6) == HTTP_VERSION_1_0[6] ) )
				{
					if( UNLIKELY( *(p2+7) == HTTP_VERSION_1_0[7] ) )
						*(p_version) = HTTP_VERSION_1_0_N ;
					else if( LIKELY( *(p2+7) == HTTP_VERSION_1_1[7] ) )
						*(p_version) = HTTP_VERSION_1_1_N ;
					else
						return FASTERHTTP_ERROR_VERSION_NOT_SUPPORTED;
				}
			}
			else
			{
				return FASTERHTTP_ERROR_VERSION_NOT_SUPPORTED;
			}
			
			*(p_parse_step) = FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_STATUSCODE0 ;
			
		case FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_STATUSCODE0 :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_STATUSCODE0\n" );
#endif
			
			for( ; UNLIKELY( (*p) == ' ' && p < fill_ptr ) ; p++ )
				;
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			if( UNLIKELY( (*p) == HTTP_NEWLINE ) )
				return FASTERHTTP_ERROR_HTTP_HEADERSTARTLINE_INVALID;
			p_STATUSCODE->p_buffer = b ;
			p_STATUSCODE->value_offset = p - b->base ;
			p++;
			
			*(p_parse_step) = FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_STATUSCODE ;
			
		case FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_STATUSCODE :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_STATUSCODE\n" );
#endif
			
			for( ; LIKELY( (*p) != ' ' && (*p) != HTTP_NEWLINE && p < fill_ptr ) ; p++ )
				;
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			if( UNLIKELY( (*p) == HTTP_NEWLINE ) )
				return FASTERHTTP_ERROR_HTTP_HEADERSTARTLINE_INVALID;
			p_STATUSCODE->value_len = p - (b->base+p_STATUSCODE->value_offset) ;
			p++;
			
			*(p_parse_step) = FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_REASONPHRASE0 ;
			
		case FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_REASONPHRASE0 :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_REASONPHRASE0\n" );
#endif
			
			for( ; UNLIKELY( (*p) == ' ' && p < fill_ptr ) ; p++ )
				;
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			if( UNLIKELY( (*p) == HTTP_NEWLINE ) )
				return FASTERHTTP_ERROR_HTTP_HEADERSTARTLINE_INVALID;
			p_REASONPHRASE->p_buffer = b ;
			p_REASONPHRASE->value_offset = p - b->base ;
			p++;
			
			*(p_parse_step) = FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_REASONPHRASE ;
			
		case FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_REASONPHRASE :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_REASONPHRASE\n" );
#endif
			
			for( ; LIKELY( (*p) != HTTP_NEWLINE && p < fill_ptr ) ; p++ )
				;
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			p_REASONPHRASE->value_len = p - (b->base+p_REASONPHRASE->value_offset) ;
			if( LIKELY( *(p-1) == HTTP_RETURN ) )
				p_REASONPHRASE->value_len--;
			p++;
			
			*(p_parse_step) = FASTERHTTP_PARSESTEP_HEADER_NAME0 ;
			goto _GOTO_PARSESTEP_HEADER_NAME0;
			
		case FASTERHTTP_PARSESTEP_HEADER_NAME0 :
			
_GOTO_PARSESTEP_HEADER_NAME0 :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_HEADER_NAME0\n" );
#endif
			
			/*
			while( UNLIKELY( (*p) == ' ' && p < fill_ptr ) )
				p++;
			*/
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			else if( LIKELY( (*p) == HTTP_RETURN ) )
			{
				if( UNLIKELY( p+1 >= fill_ptr ) )
				{
					return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
				}
				else if( LIKELY( *(p+1) == HTTP_NEWLINE ) )
				{
					p += 2 ;
				}
				else
				{
					p++;
				}
				
#define _IF_THEN_GO_PARSING_BODY \
				if( e->headers.method == HTTP_METHOD_HEAD_N ) \
				{ \
					b->process_ptr = p ; \
					*(p_parse_step) = FASTERHTTP_PARSESTEP_DONE ; \
					return 0; \
				} \
				else if( *(p_content_length) > 0 ) \
				{ \
					e->body = p ; \
					*(p_parse_step) = FASTERHTTP_PARSESTEP_BODY ; \
					goto _GOTO_PARSESTEP_BODY; \
				} \
				else if( *(p_transfer_encoding__chunked) == 1 ) \
				{ \
					e->body = p ; \
					e->chunked_body_end = p ; \
					*(p_parse_step) = FASTERHTTP_PARSESTEP_CHUNKED_SIZE ; \
					goto _GOTO_PARSESTEP_CHUNKED_SIZE; \
				} \
				else \
				{ \
					b->process_ptr = p ; \
					*(p_parse_step) = FASTERHTTP_PARSESTEP_DONE ; \
					return 0; \
				}
				
				_IF_THEN_GO_PARSING_BODY
			}
			else if( UNLIKELY( (*p) == HTTP_NEWLINE ) )
			{
				p++;
				
				_IF_THEN_GO_PARSING_BODY
			}
			else
			{
				p_header->p_buffer = b ;
				p_header->name_offset = p - b->base ;
				p++;
				*(p_parse_step) = FASTERHTTP_PARSESTEP_HEADER_NAME ;
			}
			
		case FASTERHTTP_PARSESTEP_HEADER_NAME :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_HEADER_NAME\n" );
#endif
			
			while( LIKELY( (*p) != ' ' && (*p) != ':' && (*p) != HTTP_NEWLINE && p < fill_ptr ) )
				p++;
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			if( UNLIKELY( (*p) == HTTP_NEWLINE ) )
				return FASTERHTTP_ERROR_HTTP_HEADER_INVALID;
			p_header->name_len = p - (b->base+p_header->name_offset) ;
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> HEADER-NAME [%.*s]\n" , (int)(p_header->name_len) , b->base+p_header->name_offset );
#endif
			
			while( UNLIKELY( (*p) != ':' && (*p) != HTTP_NEWLINE && p < fill_ptr ) )
				p++;
			if( UNLIKELY( (*p) == HTTP_NEWLINE ) )
				return FASTERHTTP_ERROR_HTTP_HEADER_INVALID;
			else if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			p++;
			
			*(p_parse_step) = FASTERHTTP_PARSESTEP_HEADER_VALUE0 ;
			
		case FASTERHTTP_PARSESTEP_HEADER_VALUE0 :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_HEADER_VALUE0\n" );
#endif
			
			while( LIKELY( (*p) == ' ' && p < fill_ptr ) )
				p++;
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			if( UNLIKELY( (*p) == HTTP_NEWLINE ) )
				return FASTERHTTP_ERROR_HTTP_HEADER_INVALID;
			p_header->value_offset = p - b->base ;
			p++;
			
			*(p_parse_step) = FASTERHTTP_PARSESTEP_HEADER_VALUE ;
			
		case FASTERHTTP_PARSESTEP_HEADER_VALUE :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_HEADER_VALUE\n" );
#endif
			
			while( LIKELY( (*p) != HTTP_NEWLINE && p < fill_ptr ) )
				p++;
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			p_header->value_len = p - (b->base+p_header->value_offset) ;
			p2 = p - 1 ;
			while( LIKELY( (*p2) == ' ' || (*p2) == HTTP_RETURN ) )
			{
				p_header->value_len--;
				p2--;
			}
			p++;
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> HEADER-NAME-VALUE [%.*s]:[%.*s]\n" , (int)(p_header->name_len) , b->base+p_header->name_offset , (int)(p_header->value_len) , b->base+p_header->value_offset );
#endif
			
			if( UNLIKELY( p_header->name_len == sizeof(HTTP_HEADER_CONTENT_LENGTH)-1 && STRNICMP( b->base+p_header->name_offset , == , HTTP_HEADER_CONTENT_LENGTH , sizeof(HTTP_HEADER_CONTENT_LENGTH)-1 ) ) )
			{
				*(p_content_length) = strtol( b->base+p_header->value_offset , NULL , 10 ) ;
			}
			else if( UNLIKELY( p_header->name_len == sizeof(HTTP_HEADER_TRANSFERENCODING)-1 && STRNICMP( b->base+p_header->name_offset , == , HTTP_HEADER_TRANSFERENCODING , sizeof(HTTP_HEADER_TRANSFERENCODING)-1 ) ) && UNLIKELY( p_header->value_len == sizeof(HTTP_HEADER_TRANSFERENCODING__CHUNKED)-1 && STRNICMP( b->base+p_header->value_offset , == , HTTP_HEADER_TRANSFERENCODING__CHUNKED , sizeof(HTTP_HEADER_TRANSFERENCODING__CHUNKED)-1 ) ) )
			{
				*(p_transfer_encoding__chunked) = 1 ;
			}
			else if( UNLIKELY( p_header->name_len == sizeof(HTTP_HEADER_TRAILER)-1 && STRNICMP( b->base+p_header->name_offset , == , HTTP_HEADER_TRAILER , sizeof(HTTP_HEADER_TRAILER)-1 ) ) )
			{
				p_TRAILER->p_buffer = b ;
				p_TRAILER->value_offset = p_header->value_offset ;
				p_TRAILER->value_len = p_header->value_len ;
			}
			else if( UNLIKELY( p_header->name_len == sizeof(HTTP_HEADER_CONNECTION)-1 && STRNICMP( b->base+p_header->name_offset , == , HTTP_HEADER_CONNECTION , sizeof(HTTP_HEADER_CONNECTION)-1 ) ) )
			{
				if( LIKELY( p_header->value_len == sizeof(HTTP_HEADER_CONNECTION__KEEPALIVE)-1 && STRNICMP( b->base+p_header->value_offset , == , HTTP_HEADER_CONNECTION__KEEPALIVE , sizeof(HTTP_HEADER_CONNECTION__KEEPALIVE)-1 ) ) )
					*(p_connection__keepalive) = 1 ;
				else if( LIKELY( p_header->value_len == sizeof(HTTP_HEADER_CONNECTION__CLOSE)-1 && STRNICMP( b->base+p_header->value_offset , == , HTTP_HEADER_CONNECTION__CLOSE , sizeof(HTTP_HEADER_CONNECTION__CLOSE)-1 ) ) )
					*(p_connection__keepalive) = -1 ;
				else
					*(p_connection__keepalive) = 0 ;
			}
			e->headers.header_array_count++;
			
			if( *(p_transfer_encoding__chunked) == 1 && *(p_content_length) > 0 && UNLIKELY( p_header->name_len == p_TRAILER->value_len && STRNICMP( b->base+p_header->name_offset , == , b->base+p_TRAILER->value_offset , p_header->name_len ) ) )
			{
				b->process_ptr = p ;
				*(p_parse_step) = FASTERHTTP_PARSESTEP_DONE ;
				if( b->fill_ptr > b->process_ptr )
					e->reforming_flag = 1 ;
				return 0;
			}
			
			if( UNLIKELY( e->headers.header_array_count+1 > e->headers.header_array_size ) )
			{
				int			new_header_array_size ;
				struct HttpHeader	*new_header_array ;
				
				new_header_array_size = e->headers.header_array_size * 2 ;
				new_header_array = (struct HttpHeader *)realloc( e->headers.header_array , sizeof(struct HttpHeader) * new_header_array_size ) ;
				if( new_header_array == NULL )
					return FASTERHTTP_ERROR_ALLOC;
				memset( new_header_array + e->headers.header_array_count-1 , 0x00 , sizeof(struct HttpHeader) * (new_header_array_size-e->headers.header_array_count+1) );
				e->headers.header_array_size = new_header_array_size ;
				e->headers.header_array = new_header_array ;
			}
			p_header = & (e->headers.header_array[e->headers.header_array_count]) ;
			
			*(p_parse_step) = FASTERHTTP_PARSESTEP_HEADER_NAME0 ;
			goto _GOTO_PARSESTEP_HEADER_NAME0;
			
		case FASTERHTTP_PARSESTEP_BODY :
			
_GOTO_PARSESTEP_BODY :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_BODY\n" );
#endif
			
			if( LIKELY( fill_ptr - e->body >= *(p_content_length) ) )
			{
				b->process_ptr = e->body + *(p_content_length) ;
				*(p_parse_step) = FASTERHTTP_PARSESTEP_DONE ;
				if( b->fill_ptr > b->process_ptr )
					e->reforming_flag = 1 ;
				return 0;
			}
			else
			{
				b->process_ptr = fill_ptr ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			
		case FASTERHTTP_PARSESTEP_CHUNKED_SIZE :
			
_GOTO_PARSESTEP_CHUNKED_SIZE :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_CHUNKED_SIZE\n" );
#endif
			
			for( ; UNLIKELY( (*p) != HTTP_NEWLINE && p < fill_ptr ) ; p++ )
			{
				if( LIKELY( '0' <= (*p) && (*p) <= '9' ) )
					e->chunked_length = e->chunked_length * 10 + (*p) - '0' ;
				else if( 'a' <= (*p) && (*p) <= 'f' )
					e->chunked_length = e->chunked_length * 10 + (*p) - 'a' + 10 ;
				else if( 'A' <= (*p) && (*p) <= 'F' )
					e->chunked_length = e->chunked_length * 10 + (*p) - 'A' + 10 ;
				e->chunked_length_length++;
			}
			if( UNLIKELY( p >= fill_ptr ) )
			{
				b->process_ptr = p ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			e->chunked_length_length++;
			p++;
			if( e->chunked_length == 0 )
			{
				if( p_TRAILER->value_offset )
				{
					*(p_parse_step) = FASTERHTTP_PARSESTEP_HEADER_NAME0 ;
					goto _GOTO_PARSESTEP_HEADER_NAME0;
				}
				
				b->process_ptr = p ;
				*(p_parse_step) = FASTERHTTP_PARSESTEP_DONE ;
				if( b->fill_ptr > b->process_ptr )
					e->reforming_flag = 1 ;
				return 0;
			}
			
			e->chunked_body = p ;
			*(p_parse_step) = FASTERHTTP_PARSESTEP_CHUNKED_DATA ;
			
		case FASTERHTTP_PARSESTEP_CHUNKED_DATA :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_CHUNKED_DATA\n" );
#endif
			
			if( LIKELY( fill_ptr - e->chunked_body >= e->chunked_length + 2 ) )
			{
				memmove( e->chunked_body_end , e->chunked_body , e->chunked_length );
				p = e->chunked_body + e->chunked_length ;
				e->chunked_body_end = e->chunked_body_end + e->chunked_length ;
				if( (*p) == '\r' && *(p+1) == '\n' )
					p+=2;
				else if( (*p) == '\n' )
					p++;
				
				*(p_content_length) += e->chunked_length ;
				
				*(p_parse_step) = FASTERHTTP_PARSESTEP_CHUNKED_SIZE ;
				e->chunked_length = 0 ;
				e->chunked_length_length = 0 ;
				goto _GOTO_PARSESTEP_CHUNKED_SIZE;
			}
			else
			{
				b->process_ptr = fill_ptr ;
				return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER;
			}
			
		case FASTERHTTP_PARSESTEP_DONE :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> case FASTERHTTP_PARSESTEP_DONE\n" );
#endif
			
			return 0;

		default :
#if DEBUG_PARSER
			printf( "DEBUG_PARSER >>> default\n" );
#endif
			return FASTERHTTP_ERROR_INTERNAL;
	}
}

int RequestHttp( SOCKET sock , SSL *ssl , struct HttpEnv *e )
{
	int		nret = 0 ;
	
	nret = SendHttpRequest( sock , ssl , e ) ;
	if( nret )
		return nret;
	
	nret = ReceiveHttpResponse( sock , ssl , e ) ;
	if( nret )
		return nret;
	
	return 0;
}

int ResponseAllHttp( SOCKET sock , SSL *ssl , struct HttpEnv *e , funcProcessHttpRequest *pfuncProcessHttpRequest , void *p )
{
	int		nret = 0 ;
	
	while(1)
	{
		nret = ReceiveHttpRequest( sock , ssl , e ) ;
		if( nret )
		{
			if( nret == FASTERHTTP_INFO_TCP_CLOSE )
				return 0;
			else
				return nret;
		}
		
		nret = FormatHttpResponseStartLine( HTTP_OK , e , 0 , NULL ) ;
		if( nret )
			return nret;
		
		nret = pfuncProcessHttpRequest( e , p ) ;
		if( nret != HTTP_OK )
		{
			nret = FormatHttpResponseStartLine( abs(nret)/1000 , e , 1 , NULL ) ;
			if( nret )
				return nret;
		}
		
		nret = SendHttpResponse( sock , ssl , e ) ;
		if( nret )
			return nret;
		
		if( ! CheckHttpKeepAlive(e) )
			break;
		
		ResetHttpEnv(e);
	}
	
	return 0;
}

int SendHttpRequest( SOCKET sock , SSL *ssl , struct HttpEnv *e )
{
	char			*p = NULL ;
	int			nret = 0 ;
	
	p = e->request_buffer.base ;
	if( p && p[0] == HTTP_METHOD_HEAD[0] && p[1] == HTTP_METHOD_HEAD[1] && p[2] == HTTP_METHOD_HEAD[2] && p[3] == HTTP_METHOD_HEAD[3] )
	{
		e->headers.method = HTTP_METHOD_HEAD_N ;
	}
	
	while(1)
	{
		if( e->request_buffer.process_ptr < e->request_buffer.fill_ptr )
		{
			nret = SendHttpBuffer( sock , ssl , e , & (e->request_buffer) , 1 ) ;
			if( nret == 0 )
			{
				if( e->p_append_buffer == NULL )
					return 0;
				else
					goto _CONTINUE_TO_SEND;
			}
			else if( nret != FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK )
			{
				return nret;
			}
		}
		else if( e->p_append_buffer && e->p_append_buffer->process_ptr < e->p_append_buffer->fill_ptr )
		{
_CONTINUE_TO_SEND :
			nret = SendHttpBuffer( sock , ssl , e , e->p_append_buffer , 1 ) ;
			if( nret == 0 )
			{
				return 0;
			}
			else if( nret != FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK )
			{
				return nret;
			}
		}
		else
		{
			return 0;
		}
	}
}

static int UncompressBuffer( struct HttpEnv *e , struct HttpBuffer *b , char *p_compress_algorithm , int compress_algorithm_len , int compress_algorithm )
{
	uLong		in_len ;
	uLong		out_len ;
	Bytef		*out_base = NULL ;
	z_stream	stream ;
	
	int		new_buf_size ;
	
	int		nret = 0 ;
	
	in_len = e->headers.content_length ;
	out_len = in_len * 10 ;
	out_base = (Bytef*)malloc( out_len+1 ) ;
	if( out_base == NULL )
	{
		return FASTERHTTP_ERROR_ALLOC;
	}
	memset( out_base , 0x00 , out_len+1 );
	
	memset( & stream , 0x00 , sizeof(z_stream) );
	stream.zalloc = NULL ;
	stream.zfree = NULL ;
	stream.opaque = NULL ;
	nret = inflateInit2( &stream , compress_algorithm ) ;
	if( nret != Z_OK )
	{
		free( out_base );
		return FASTERHTTP_ERROR_ZLIB__+nret;
	}
	
	stream.next_in = (Bytef*)(e->body) ;
	stream.avail_in = in_len ;
	while(1)
	{
		stream.next_out = out_base + stream.total_out ;
		stream.avail_out = out_len - stream.total_out ;
		nret = inflate( &stream , Z_NO_FLUSH ) ;
		if( nret != Z_OK && nret != Z_STREAM_END )
		{
			free( out_base );
			inflateEnd( &stream );
			return FASTERHTTP_ERROR_ZLIB__+nret;
		}
		else
		{
			uLong		new_out_len ;
			Bytef		*new_out_base = NULL ;
			
			if( stream.avail_in == 0 )
				break;
			
			new_out_len = out_len * 2 ;
			new_out_base = (Bytef*)realloc( out_base , new_out_len+1 ) ;
			if( new_out_base == NULL )
			{
				free( out_base );
				inflateEnd( &stream );
				return FASTERHTTP_ERROR_ALLOC;
			}
			memset( new_out_base + stream.total_out , 0x00 , new_out_len - stream.total_out );
			out_len = new_out_len ;
			out_base = new_out_base ;
		}
	}
	
	new_buf_size = (e->body-b->base) + stream.total_out + 1 ;
	if( new_buf_size > b->buf_size )
	{
		nret = ReallocHttpBuffer( b , new_buf_size ) ;
		if( nret )
		{
			free( out_base );
			inflateEnd( &stream );
			return nret;
		}
	}
	
	memmove( e->body , out_base , stream.total_out );
	b->fill_ptr = e->body + stream.total_out ;
	e->headers.content_length = stream.total_out ;
	
	free( out_base );
	nret = inflateEnd( &stream ) ;
	if( nret != Z_OK )
	{
		return FASTERHTTP_ERROR_ZLIB__+nret;
	}
	
	return 0;
}

static int UncompressHttpBody( struct HttpEnv *e , char *header )
{
	char	*base = NULL ;
	char	*p_compress_algorithm = NULL ;
	int	compress_algorithm_len ;
	
	base = QueryHttpHeaderPtr( e , header , NULL ) ;
	while( base )
	{
		base = TokenHttpHeaderValue( base , & p_compress_algorithm , & compress_algorithm_len ) ;
		if( p_compress_algorithm )
		{
			if( compress_algorithm_len == 4 && STRNICMP( p_compress_algorithm , == , "gzip" , compress_algorithm_len ) )
			{
				return UncompressBuffer( e , GetHttpResponseBuffer(e) , p_compress_algorithm , compress_algorithm_len , HTTP_COMPRESSALGORITHM_GZIP ) ;
			}
			else if( compress_algorithm_len == 7 && STRNICMP( p_compress_algorithm , == , "deflate" , compress_algorithm_len ) )
			{
				return UncompressBuffer( e , GetHttpResponseBuffer(e) , p_compress_algorithm , compress_algorithm_len , HTTP_COMPRESSALGORITHM_DEFLATE ) ;
			}
		}
	}
	
	return 0;
}

static int CompressBuffer( struct HttpEnv *e , struct HttpBuffer *b , char *p_compress_algorithm , int compress_algorithm_len , int compress_algorithm )
{
	char		*p_CONTENT_LENGTH = NULL ;
	char		*p_CONTENT_LENGTH_end = NULL ;
	char		*p_headers_end = NULL ;
	char		*body = NULL ;
	
	uLong		in_len ;
	uLong		out_len ;
	Bytef		*out_base = NULL ;
	z_stream	stream ;
	
	int		new_buf_size ;
	
	int		nret = 0 ;
	
	p_CONTENT_LENGTH = STRISTR( b->base , HTTP_HEADER_CONTENT_LENGTH ) ;
	if( p_CONTENT_LENGTH == NULL || p_CONTENT_LENGTH >= b->fill_ptr )
		return 0;
	p_CONTENT_LENGTH_end = strstr( p_CONTENT_LENGTH+1 , "\r\n" ) ;
	if( p_CONTENT_LENGTH_end == NULL || p_CONTENT_LENGTH_end >= b->fill_ptr )
		return 0;
	p_CONTENT_LENGTH_end += 2 ;
	p_headers_end = strstr( p_CONTENT_LENGTH+1 , "\r\n\r\n" ) ;
	if( p_headers_end == NULL || p_headers_end >= b->fill_ptr )
		return 0;
	p_headers_end += 2 ;
	body = p_headers_end + 2 ;
	
	stream.zalloc = NULL ;
	stream.zfree = NULL ;
	stream.opaque = NULL ;
	nret = deflateInit2( &stream , Z_DEFAULT_COMPRESSION , Z_DEFLATED , compress_algorithm , MAX_MEM_LEVEL , Z_DEFAULT_STRATEGY ) ;
	if( nret != Z_OK )
	{
		return FASTERHTTP_ERROR_ZLIB__+nret;
	}
	
	in_len = b->fill_ptr - body ;
	out_len = deflateBound( &stream , in_len ) ;
	out_base = (unsigned char *)malloc( out_len+1 ) ;
	if( out_base == NULL )
	{
		deflateEnd( &stream );
		return FASTERHTTP_ERROR_ALLOC;
	}
	
	stream.next_in = (Bytef*)body ;
	stream.avail_in = in_len ;
	stream.next_out = out_base ;
	stream.avail_out = out_len ;
	nret = deflate( &stream , Z_FINISH ) ;
	if( nret != Z_OK && nret != Z_STREAM_END )
	{
		free( out_base );
		deflateEnd( &stream );
		return FASTERHTTP_ERROR_ZLIB__+nret;
	}
	
	new_buf_size = (body-b->base) + strlen(HTTP_HEADER_CONTENT_LENGTH)+2+10+2 + strlen(HTTP_HEADER_CONTENTENCODING)+2+10+2 + 10+2 + stream.total_out + 1 ;
	if( new_buf_size > b->buf_size )
	{
		nret = ReallocHttpBuffer( b , new_buf_size ) ;
		if( nret )
		{
			free( out_base );
			deflateEnd( &stream );
			return nret;
		}
		
		p_CONTENT_LENGTH = STRISTR( b->base , HTTP_HEADER_CONTENT_LENGTH ) ;
		if( p_CONTENT_LENGTH == NULL || p_CONTENT_LENGTH >= b->fill_ptr )
			return 0;
		p_CONTENT_LENGTH_end = strstr( p_CONTENT_LENGTH+1 , "\r\n" ) ;
		if( p_CONTENT_LENGTH_end == NULL || p_CONTENT_LENGTH_end >= b->fill_ptr )
			return 0;
		p_CONTENT_LENGTH_end += 2 ;
		p_headers_end = strstr( p_CONTENT_LENGTH+1 , "\r\n\r\n" ) ;
		if( p_headers_end == NULL || p_headers_end >= b->fill_ptr )
			return 0;
		p_headers_end += 2 ;
		body = p_headers_end + 2 ;
	}
	
	memmove( p_CONTENT_LENGTH , p_CONTENT_LENGTH_end , p_headers_end - p_CONTENT_LENGTH_end );
	b->fill_ptr = p_CONTENT_LENGTH + ( p_headers_end - p_CONTENT_LENGTH_end ) ;
	nret = StrcatfHttpBuffer( b , HTTP_HEADER_CONTENTENCODING ": %.*s" HTTP_RETURN_NEWLINE
					HTTP_HEADER_CONTENT_LENGTH ": %d" HTTP_RETURN_NEWLINE
					HTTP_RETURN_NEWLINE
					, compress_algorithm_len , p_compress_algorithm
					, stream.total_out ) ;
	if( nret )
	{
		free( out_base );
		deflateEnd( &stream );
		return nret;
	}
	nret = MemcatHttpBuffer( b , (char*)out_base , (int)(stream.total_out) ) ;
	if( nret )
	{
		free( out_base );
		deflateEnd( &stream );
		return nret;
	}
	
	free( out_base );
	nret = deflateEnd( &stream ) ;
	if( nret != Z_OK )
	{
		return FASTERHTTP_ERROR_ZLIB__+nret;
	}
	
	return 0;
}

static int CompressHttpBody( struct HttpEnv *e , char *header )
{
	char	*base = NULL ;
	char	*p_compress_algorithm = NULL ;
	int	compress_algorithm_len ;
	
	base = QueryHttpHeaderPtr( e , header , NULL ) ;
	while( base )
	{
		base = TokenHttpHeaderValue( base , & p_compress_algorithm , & compress_algorithm_len ) ;
		if( p_compress_algorithm )
		{
			if( compress_algorithm_len == 4 && STRNICMP( p_compress_algorithm , == , "gzip" , compress_algorithm_len ) )
			{
				return CompressBuffer( e , GetHttpResponseBuffer(e) , p_compress_algorithm , compress_algorithm_len , HTTP_COMPRESSALGORITHM_GZIP ) ;
			}
			else if( compress_algorithm_len == 7 && STRNICMP( p_compress_algorithm , == , "deflate" , compress_algorithm_len ) )
			{
				return CompressBuffer( e , GetHttpResponseBuffer(e) , p_compress_algorithm , compress_algorithm_len , HTTP_COMPRESSALGORITHM_DEFLATE ) ;
			}
		}
	}
	
	return 0;
}

int ReceiveHttpResponse( SOCKET sock , SSL *ssl , struct HttpEnv *e )
{
	int		nret = 0 ;
	
	while(1)
	{
		nret = ReceiveHttpBuffer( sock , ssl , e , & (e->response_buffer) , 1 ) ;
		if( nret )
		{
			if( nret == FASTERHTTP_ERROR_TCP_CLOSE && CheckHttpKeepAlive(e) && e->parse_step == FASTERHTTP_PARSESTEP_BEGIN )
				return FASTERHTTP_INFO_TCP_CLOSE;
			else
				return nret;
		}
		
		nret = ParseHttpBuffer( e , & (e->response_buffer) ) ;
		if( nret == FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER )
			;
		else if( nret )
			return nret;
		else
			return UncompressHttpBody( e , HTTP_HEADER_CONTENTENCODING );
	}
}

int ReceiveHttpRequest( SOCKET sock , SSL *ssl , struct HttpEnv *e )
{
	int		nret = 0 ;
	
	while(1)
	{
		nret = ReceiveHttpBuffer( sock , ssl , e , & (e->request_buffer) , 1 ) ;
		if( nret )
		{
			if( nret == FASTERHTTP_ERROR_TCP_CLOSE && CheckHttpKeepAlive(e) && e->parse_step == FASTERHTTP_PARSESTEP_BEGIN )
				return FASTERHTTP_INFO_TCP_CLOSE;
			else
				return nret;
		}
		
		nret = ParseHttpBuffer( e , & (e->request_buffer) ) ;
		if( nret == FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER )
			;
		else if( nret )
			return nret;
		else
			break;
	}
	
	return 0;
}

static struct HttpBuffer	__fasterhttp_status_code_httpbuf ;

int FormatHttpResponseStartLine( int status_code , struct HttpEnv *e , int fill_body_with_status_flag , char *format , ... )
{
	struct HttpBuffer	*b = GetHttpResponseBuffer(e) ;
	
	char			*p_status_code_s = NULL ;
	char			*p_status_text = NULL ;
	
	int			nret = 0 ;
	
	if( status_code <= 0 )
		status_code = HTTP_INTERNAL_SERVER_ERROR ;
	
	switch( status_code )
	{
		case HTTP_CONTINUE :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_CONTINUE_S " " HTTP_CONTINUE_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_SWITCHING_PROTOCOL :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_SWITCHING_PROTOCOL_S " " HTTP_SWITCHING_PROTOCOL_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_OK :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_OK_S " " HTTP_OK_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_CREATED :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_CREATED_S " " HTTP_CREATED_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_ACCEPTED :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_ACCEPTED_S " " HTTP_ACCEPTED_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_NON_AUTHORITATIVE_INFORMATION :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_NON_AUTHORITATIVE_INFORMATION_S " " HTTP_NON_AUTHORITATIVE_INFORMATION_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_NO_CONTENT :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_NO_CONTENT_S " " HTTP_NO_CONTENT_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_RESET_CONTENT :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_RESET_CONTENT_S " " HTTP_RESET_CONTENT_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_PARTIAL_CONTENT :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_PARTIAL_CONTENT_S " " HTTP_PARTIAL_CONTENT_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_MULTIPLE_CHOICES :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_MULTIPLE_CHOICES_S " " HTTP_MULTIPLE_CHOICES_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_MOVED_PERMANNETLY :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_MOVED_PERMANNETLY_S " " HTTP_MOVED_PERMANNETLY_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_FOUND :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_FOUND_S " " HTTP_FOUND_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_SEE_OTHER :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_SEE_OTHER_S " " HTTP_SEE_OTHER_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_NOT_MODIFIED :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_NOT_MODIFIED_S " " HTTP_NOT_MODIFIED_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_USE_PROXY :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_USE_PROXY_S " " HTTP_USE_PROXY_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_TEMPORARY_REDIRECT :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_TEMPORARY_REDIRECT_S " " HTTP_TEMPORARY_REDIRECT_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_BAD_REQUEST :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_BAD_REQUEST_S " " HTTP_BAD_REQUEST_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_UNAUTHORIZED :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_UNAUTHORIZED_S " " HTTP_UNAUTHORIZED_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_PAYMENT_REQUIRED :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_PAYMENT_REQUIRED_S " " HTTP_PAYMENT_REQUIRED_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_FORBIDDEN :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_FORBIDDEN_S " " HTTP_FORBIDDEN_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_NOT_FOUND :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_NOT_FOUND_S " " HTTP_NOT_FOUND_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_METHOD_NOT_ALLOWED :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_METHOD_NOT_ALLOWED_S " " HTTP_METHOD_NOT_ALLOWED_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_NOT_ACCEPTABLE :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_NOT_ACCEPTABLE_S " " HTTP_NOT_ACCEPTABLE_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_PROXY_AUTHENTICATION_REQUIRED :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_PROXY_AUTHENTICATION_REQUIRED_S " " HTTP_PROXY_AUTHENTICATION_REQUIRED_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_REQUEST_TIMEOUT :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_REQUEST_TIMEOUT_S " " HTTP_REQUEST_TIMEOUT_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_CONFLICT :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_CONFLICT_S " " HTTP_CONFLICT_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_GONE :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_GONE_S " " HTTP_GONE_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_LENGTH_REQUIRED :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_LENGTH_REQUIRED_S " " HTTP_LENGTH_REQUIRED_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_PRECONDITION_FAILED :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_PRECONDITION_FAILED_S " " HTTP_PRECONDITION_FAILED_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_REQUEST_ENTITY_TOO_LARGE :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_REQUEST_ENTITY_TOO_LARGE_S " " HTTP_REQUEST_ENTITY_TOO_LARGE_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_URI_TOO_LONG :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_URI_TOO_LONG_S " " HTTP_URI_TOO_LONG_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_UNSUPPORTED_MEDIA_TYPE :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_UNSUPPORTED_MEDIA_TYPE_S " " HTTP_UNSUPPORTED_MEDIA_TYPE_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_REQUESTED_RANGE_NOT_SATISFIABLE :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_REQUESTED_RANGE_NOT_SATISFIABLE_S " " HTTP_REQUESTED_RANGE_NOT_SATISFIABLE_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_EXPECTATION_FAILED :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_EXPECTATION_FAILED_S " " HTTP_EXPECTATION_FAILED_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_INTERNAL_SERVER_ERROR :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_INTERNAL_SERVER_ERROR_S " " HTTP_INTERNAL_SERVER_ERROR_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_NOT_IMPLEMENTED :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_NOT_IMPLEMENTED_S " " HTTP_NOT_IMPLEMENTED_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_BAD_GATEWAY :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_BAD_GATEWAY_S " " HTTP_BAD_GATEWAY_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_SERVICE_UNAVAILABLE :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_SERVICE_UNAVAILABLE_S " " HTTP_SERVICE_UNAVAILABLE_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_GATEWAY_TIMEOUT :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_GATEWAY_TIMEOUT_S " " HTTP_GATEWAY_TIMEOUT_T HTTP_RETURN_NEWLINE ) ;
			break;
		case HTTP_VERSION_NOT_SUPPORTED :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_VERSION_NOT_SUPPORTED_S " " HTTP_VERSION_NOT_SUPPORTED_T HTTP_RETURN_NEWLINE ) ;
			break;
		default :
			nret = StrcpyHttpBuffer( b , HTTP_VERSION_1_1 " " HTTP_INTERNAL_SERVER_ERROR_S " " HTTP_INTERNAL_SERVER_ERROR_T HTTP_RETURN_NEWLINE ) ;
			break;
	}
	if( nret )
		return nret;
	
	if( format )
	{
		va_list		valist ;
		va_start( valist , format );
		nret = StrcatvHttpBuffer( b , format , valist ) ;
		va_end( valist );
		if( nret )
			return nret;
	}
	
	GetHttpStatus( status_code , &p_status_code_s , &p_status_text );
	memset( & __fasterhttp_status_code_httpbuf , 0x00 , sizeof(__fasterhttp_status_code_httpbuf) );
	__fasterhttp_status_code_httpbuf.base = p_status_code_s ;
	e->headers.STATUSCODE.p_buffer = &__fasterhttp_status_code_httpbuf ;
	e->headers.STATUSCODE.value_offset = 0 ;
	e->headers.STATUSCODE.value_len = 3 ;
	
	if( fill_body_with_status_flag )
	{
		nret = StrcatfHttpBuffer( b	, "Content-length: %d%s"
						"%s"
						"%s %s"
						, strlen(p_status_text) , HTTP_RETURN_NEWLINE
						, HTTP_RETURN_NEWLINE
						, p_status_text
						) ;
		if( nret )
			return nret;
	}
	
	return 0;
}

int SendHttpRequestNonblock( SOCKET sock , SSL *ssl , struct HttpEnv *e )
{
	char			*p = NULL ;
	int			nret = 0 ;
	
	p = e->request_buffer.base ;
	if( p && p[0] == HTTP_METHOD_HEAD[0] && p[1] == HTTP_METHOD_HEAD[1] && p[2] == HTTP_METHOD_HEAD[2] && p[3] == HTTP_METHOD_HEAD[3] )
	{
		e->headers.method = HTTP_METHOD_HEAD_N ;
	}
	
	if( e->request_buffer.process_ptr < e->request_buffer.fill_ptr )
	{
		nret = SendHttpBuffer( sock , ssl , e , & (e->request_buffer) , 0 ) ;
		if( nret == 0 )
		{
			if( e->p_append_buffer == NULL )
				return 0;
			else
				goto _CONTINUE_TO_SEND;
		}
		else if( nret != FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK )
		{
			return nret;
		}
	}
	else if( e->p_append_buffer && e->p_append_buffer->process_ptr < e->p_append_buffer->fill_ptr )
	{
_CONTINUE_TO_SEND :
		nret = SendHttpBuffer( sock , ssl , e , e->p_append_buffer , 0 ) ;
		if( nret == 0 )
		{
			return 0;
		}
		else if( nret != FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK )
		{
			return nret;
		}
	}
	else
	{
		return 0;
	}
	
	return FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK;
}

int ReceiveHttpResponseNonblock( SOCKET sock , SSL *ssl , struct HttpEnv *e )
{
	int		nret = 0 ;
	
	while(1)
	{
		nret = ReceiveHttpBuffer( sock , ssl , e , & (e->response_buffer) , 0 ) ;
		if( nret )
		{
			if( nret == FASTERHTTP_ERROR_TCP_CLOSE && CheckHttpKeepAlive(e) && e->parse_step == FASTERHTTP_PARSESTEP_BEGIN )
				return FASTERHTTP_INFO_TCP_CLOSE;
			else
				return nret;
		}
		
		nret = ParseHttpBuffer( e , & (e->response_buffer) ) ;
		if( nret == FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER )
			;
		else if( nret )
			return nret;
		else
			return UncompressHttpBody( e , HTTP_HEADER_CONTENTENCODING );
	}
}

_WINDLL_FUNC int ReceiveHttpResponseNonblock1( SOCKET sock , SSL *ssl , struct HttpEnv *e )
{
	int		nret = 0 ;
	
	while(1)
	{
		nret = ReceiveHttpBuffer1( sock , ssl , e , & (e->response_buffer) , 0 ) ;
		if( nret )
		{
			if( nret == FASTERHTTP_ERROR_TCP_CLOSE && CheckHttpKeepAlive(e) && e->parse_step == FASTERHTTP_PARSESTEP_BEGIN )
				return FASTERHTTP_INFO_TCP_CLOSE;
			else
				return nret;
		}
		
		nret = ParseHttpBuffer( e , & (e->response_buffer) ) ;
		if( nret == FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER )
			;
		else if( nret )
			return nret;
		else
			return UncompressHttpBody( e , HTTP_HEADER_CONTENTENCODING );
	}
}

int ReceiveHttpRequestNonblock( SOCKET sock , SSL *ssl , struct HttpEnv *e )
{
	int		nret = 0 ;
	
	while(1)
	{
		nret = ReceiveHttpBuffer( sock , ssl , e , & (e->request_buffer) , 0 ) ;
		if( nret )
		{
			if( nret == FASTERHTTP_ERROR_TCP_CLOSE && CheckHttpKeepAlive(e) && e->parse_step == FASTERHTTP_PARSESTEP_BEGIN )
				return FASTERHTTP_INFO_TCP_CLOSE;
			else
				return nret;
		}
		
		nret = ParseHttpBuffer( e , & (e->request_buffer) ) ;
		if( nret == FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER )
			;
		else if( nret )
			return nret;
		else
			return 0;
	}
}

_WINDLL_FUNC int ReceiveHttpRequestNonblock1( SOCKET sock , SSL *ssl , struct HttpEnv *e )
{
	int		nret = 0 ;
	
	while(1)
	{
		nret = ReceiveHttpBuffer1( sock , ssl , e , & (e->request_buffer) , 0 ) ;
		if( nret )
		{
			if( nret == FASTERHTTP_ERROR_TCP_CLOSE && CheckHttpKeepAlive(e) && e->parse_step == FASTERHTTP_PARSESTEP_BEGIN )
				return FASTERHTTP_INFO_TCP_CLOSE;
			else
				return nret;
		}
		
		nret = ParseHttpBuffer( e , & (e->request_buffer) ) ;
		if( nret == FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER )
			;
		else if( nret )
			return nret;
		else
			return 0;
	}
}

int SendHttpResponseNonblock( SOCKET sock , SSL *ssl , struct HttpEnv *e )
{
	int		nret = 0 ;
	 
	if( e->enable_response_compressing == 1 )
	{
		nret = CompressHttpBody( e , HTTP_HEADER_ACCEPTENCODING ) ;
		if( nret )
			return nret;
		
		e->enable_response_compressing = 2 ;
	}
	
	if( e->response_buffer.process_ptr < e->response_buffer.fill_ptr )
	{
		nret = SendHttpBuffer( sock , ssl , e , & (e->response_buffer) , 0 ) ;
		if( nret == 0 )
		{
			if( e->p_append_buffer == NULL )
				return 0;
			else
				goto _CONTINUE_TO_SEND;
		}
		else if( nret != FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK )
		{
			return nret;
		}
	}
	else if( e->p_append_buffer && e->p_append_buffer->process_ptr < e->p_append_buffer->fill_ptr )
	{
_CONTINUE_TO_SEND :
		nret = SendHttpBuffer( sock , ssl , e , e->p_append_buffer , 0 ) ;
		if( nret == 0 )
		{
			return 0;
		}
		else if( nret != FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK )
		{
			return nret;
		}
	}
	else
	{
		return 0;
	}
	
	return FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK;
}

int SendHttpResponseNonblock1( SOCKET sock , SSL *ssl , struct HttpEnv *e )
{
	int		nret = 0 ;
	 
	if( e->enable_response_compressing == 1 )
	{
		nret = CompressHttpBody( e , HTTP_HEADER_ACCEPTENCODING ) ;
		if( nret )
			return nret;
		
		e->enable_response_compressing = 2 ;
	}
	
	if( e->response_buffer.process_ptr < e->response_buffer.fill_ptr )
	{
		nret = SendHttpBuffer1( sock , ssl , e , & (e->response_buffer) , 0 ) ;
		if( nret == 0 )
		{
			if( e->p_append_buffer == NULL )
				return 0;
			else
				goto _CONTINUE_TO_SEND;
		}
		else if( nret != FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK )
		{
			return nret;
		}
	}
	else if( e->p_append_buffer && e->p_append_buffer->process_ptr < e->p_append_buffer->fill_ptr )
	{
_CONTINUE_TO_SEND :
		nret = SendHttpBuffer1( sock , ssl , e , e->p_append_buffer , 0 ) ;
		if( nret == 0 )
		{
			return 0;
		}
		else if( nret != FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK )
		{
			return nret;
		}
	}
	else
	{
		return 0;
	}
	
	return FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK;
}

int CheckHttpKeepAlive( struct HttpEnv *e )
{
	if(	( e->headers.version == HTTP_VERSION_1_0_N && e->headers.connection__keepalive == 1 )
		||
		( e->headers.version == HTTP_VERSION_1_1_N && e->headers.connection__keepalive != -1 )
	)
		return 1;
	else
		return 0;
}

void SetHttpKeepAlive( struct HttpEnv *e , char keepalive )
{
	if( keepalive )
	{
		e->headers.connection__keepalive = 1 ;
	}
	else
	{
		if( e->headers.version == HTTP_VERSION_1_0_N )
			e->headers.connection__keepalive = 0 ;
		else if( e->headers.version == HTTP_VERSION_1_1_N )
			e->headers.connection__keepalive = -1 ;
		else
			e->headers.connection__keepalive = 0 ;
	}
	
	return;
}

int ParseHttpResponse( struct HttpEnv *e )
{
	int		nret = 0 ;
	
	nret = ParseHttpBuffer( e , & (e->response_buffer) ) ;
	if( nret )
		return nret;
	
	return 0;
}

int ParseHttpRequest( struct HttpEnv *e )
{
	int		nret = 0 ;
	
	nret = ParseHttpBuffer( e , & (e->request_buffer) ) ;
	if( nret )
		return nret;
	
	return 0;
}

#undef DEBUG_PARSER

#undef DEBUG_COMM

char *TokenHttpHeaderValue( char *str , char **pp_token , int *p_token_len )
{
	char	*p = str ;
	
	while( (*p) == ' ' || (*p) == ',' )
		p++;
	if( (*p) == '\r' || (*p) == '\n' || (*p) == '\0' )
	{
		(*pp_token) = NULL ;
		(*p_token_len) = 0 ;
		return NULL;
	}
	
	(*pp_token) = p ;
	while( (*p) != ' ' && (*p) != ',' && (*p) != '\r' && (*p) != '\n' && (*p) != '\0' )
		p++;
	if( p == (*pp_token) )
	{
		(*pp_token) = NULL ;
		(*p_token_len) = 0 ;
		return NULL;
	}
	else
	{
		(*p_token_len) = p - (*pp_token) ;
		return p;
	}
}

int SendHttpResponse( SOCKET sock , SSL *ssl , struct HttpEnv *e )
{
	int		nret = 0 ;
	
	if( e->enable_response_compressing == 1 )
	{
		nret = CompressHttpBody( e , HTTP_HEADER_ACCEPTENCODING ) ;
		if( nret )
			return nret;
		
		e->enable_response_compressing = 2 ;
	}
	
	while(1)
	{
		if( e->response_buffer.process_ptr < e->response_buffer.fill_ptr )
		{
			nret = SendHttpBuffer( sock , ssl , e , & (e->response_buffer) , 1 ) ;
			if( nret == 0 )
			{
				if( e->p_append_buffer == NULL )
					return 0;
				else
					goto _CONTINUE_TO_SEND;
			}
			else if( nret != FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK )
			{
				return nret;
			}
		}
		else if( e->p_append_buffer && e->p_append_buffer->process_ptr < e->p_append_buffer->fill_ptr )
		{
_CONTINUE_TO_SEND :
			nret = SendHttpBuffer( sock , ssl , e , e->p_append_buffer , 1 ) ;
			if( nret == 0 )
			{
				return 0;
			}
			else if( nret != FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK )
			{
				return nret;
			}
		}
		else
		{
			return 0;
		}
	}
	
	return 0;
}

char *GetHttpHeaderPtr_METHOD( struct HttpEnv *e , int *p_value_len )
{
	if( p_value_len )
		(*p_value_len) = e->headers.METHOD.value_len ;
	if( e->headers.METHOD.p_buffer == NULL )
		return NULL;
	else
		return e->headers.METHOD.p_buffer->base+e->headers.METHOD.value_offset;
}

int GetHttpHeaderLen_METHOD( struct HttpEnv *e )
{
	return e->headers.METHOD.value_len;
}

char *GetHttpHeaderPtr_URI( struct HttpEnv *e , int *p_value_len )
{
	if( p_value_len )
		(*p_value_len) = e->headers.URI.value_len ;
	if( e->headers.URI.p_buffer == NULL )
		return NULL;
	else
		return e->headers.URI.p_buffer->base+e->headers.URI.value_offset;
}

int GetHttpHeaderLen_URI( struct HttpEnv *e )
{
	return e->headers.URI.value_len;
}

char *GetHttpHeaderPtr_VERSION( struct HttpEnv *e , int *p_value_len )
{
	if( p_value_len )
		(*p_value_len) = e->headers.VERSION.value_len ;
	if( e->headers.VERSION.p_buffer == NULL )
		return NULL;
	else
		return e->headers.VERSION.p_buffer->base+e->headers.VERSION.value_offset;
}

int GetHttpHeaderLen_VERSION( struct HttpEnv *e )
{
	return e->headers.VERSION.value_len;
}

char *GetHttpHeaderPtr_STATUSCODE( struct HttpEnv *e , int *p_value_len )
{
	if( p_value_len )
		(*p_value_len) = e->headers.STATUSCODE.value_len ;
	if( e->headers.STATUSCODE.p_buffer == NULL )
		return NULL;
	else
		return e->headers.STATUSCODE.p_buffer->base+e->headers.STATUSCODE.value_offset;
}

int GetHttpHeaderLen_STATUSCODE( struct HttpEnv *e )
{
	return e->headers.STATUSCODE.value_len;
}

char *GetHttpHeaderPtr_REASONPHRASE( struct HttpEnv *e , int *p_value_len )
{
	if( p_value_len )
		(*p_value_len) = e->headers.REASONPHRASE.value_len ;
	if( e->headers.REASONPHRASE.p_buffer == NULL )
		return NULL;
	else
		return e->headers.REASONPHRASE.p_buffer->base+e->headers.REASONPHRASE.value_offset;
}

int GetHttpHeaderLen_REASONPHRASE( struct HttpEnv *e )
{
	return e->headers.REASONPHRASE.value_len;
}

char GetHttpHeader_METHOD( struct HttpEnv *e )
{
	return e->headers.method;
}

char GetHttpHeader_VERSION( struct HttpEnv *e )
{
	return e->headers.version;
}

struct HttpHeader *QueryHttpHeader( struct HttpEnv *e , char *name )
{
	int			i ;
	struct HttpHeader	*p_header = NULL ;
	int			name_len ;
	
	name_len = strlen(name) ;
	for( i = 0 , p_header = e->headers.header_array ; i < e->headers.header_array_size ; i++ , p_header++ )
	{
		if( p_header->p_buffer && p_header->p_buffer->base && p_header->name_len == name_len && MEMCMP( p_header->p_buffer->base+p_header->name_offset , == , name , name_len ) )
		{
			return p_header;
		}
	}
	
	return NULL;
}

char *QueryHttpHeaderPtr( struct HttpEnv *e , char *name , int *p_value_len )
{
	struct HttpHeader	*p_header = NULL ;
	
	p_header = QueryHttpHeader( e , name ) ;
	if( p_header == NULL )
		return NULL;
	
	if( p_value_len )
		(*p_value_len) = p_header->value_len ;
	if( p_header->p_buffer == NULL )
		return NULL;
	else
		return p_header->p_buffer->base+p_header->value_offset;
}

int QueryHttpHeaderLen( struct HttpEnv *e , char *name )
{
	struct HttpHeader	*p_header = NULL ;
	
	p_header = QueryHttpHeader( e , name ) ;
	if( p_header == NULL )
		return 0;
	
	return p_header->value_len;
}

int CountHttpHeaders( struct HttpEnv *e )
{
	return e->headers.header_array_count;
}

struct HttpHeader *TravelHttpHeaderPtr( struct HttpEnv *e , struct HttpHeader *p_header )
{
	if( p_header == NULL )
		p_header = e->headers.header_array ;
	else
		p_header++;
	
	for( ; p_header < e->headers.header_array + e->headers.header_array_size ; p_header++ )
	{
		if( p_header->name_offset )
		{
			return p_header;
		}
	}
	
	return NULL;
}

char *GetHttpHeaderNamePtr( struct HttpHeader *p_header , int *p_name_len )
{
	if( p_header == NULL )
		return NULL;
	
	if( p_name_len )
		(*p_name_len) = p_header->name_len ;
	if( p_header->p_buffer == NULL )
		return NULL;
	else
		return p_header->p_buffer->base+p_header->name_offset;
}

int GetHttpHeaderNameLen( struct HttpHeader *p_header )
{
	return p_header->name_len;
}

char *GetHttpHeaderValuePtr( struct HttpHeader *p_header , int *p_value_len )
{
	if( p_header == NULL )
		return NULL;
	
	if( p_value_len )
		(*p_value_len) = p_header->value_len ;
	if( p_header->p_buffer == NULL )
		return NULL;
	else
		return p_header->p_buffer->base+p_header->value_offset;
}

int GetHttpHeaderValueLen( struct HttpHeader *p_header )
{
	return p_header->value_len;
}

char *GetHttpBodyPtr( struct HttpEnv *e , int *p_body_len )
{
	if( p_body_len )
		(*p_body_len) = e->headers.content_length ;
	
	if( e->headers.content_length > 0 )
	{
		return e->body;
	}
	else
	{
		return NULL;
	}
}

int GetHttpBodyLen( struct HttpEnv *e )
{
	return e->headers.content_length;
}

struct HttpBuffer *GetHttpRequestBuffer( struct HttpEnv *e )
{
	return & (e->request_buffer);
}

struct HttpBuffer *GetHttpResponseBuffer( struct HttpEnv *e )
{
	return & (e->response_buffer);
}

struct HttpBuffer *GetHttpAppendBuffer( struct HttpEnv *e )
{
	return e->p_append_buffer;
}

char *GetHttpBufferBase( struct HttpBuffer *b , int *p_data_len )
{
	if( p_data_len )
		(*p_data_len) = b->fill_ptr - b->base ;
	
	return b->base;
}

int GetHttpBufferLength( struct HttpBuffer *b )
{
	return b->fill_ptr-b->base;
}

int GetHttpBufferSize( struct HttpBuffer *b )
{
	return b->buf_size;
}

int StrcpyHttpBuffer( struct HttpBuffer *b , char *str )
{
	int		len ;
	
	int		nret = 0 ;
	
	len = strlen(str) ;
	while( len > b->buf_size-1 )
	{
		nret = ReallocHttpBuffer( b , -1 ) ;
		if( nret )
			return nret;
	}
	
	memcpy( b->base , str , len );
	b->fill_ptr = b->base + len ;
	
	return 0;
}

int StrcpyfHttpBuffer( struct HttpBuffer *b , char *format , ... )
{
	va_list		valist ;
	int		len ;
	
	int		nret = 0 ;
	
	while(1)
	{
		va_start( valist , format );
		len = VSNPRINTF( b->base , b->buf_size-1 , format , valist ) ;
		va_end( valist );
		if( len == -1 || len == b->buf_size-1 )
		{
			nret = ReallocHttpBuffer( b , -1 ) ;
			if( nret )
				return nret;
		}
		else
		{
			b->fill_ptr = b->base + len ;
			break;
		}
	}
	
	return 0;
}

int StrcpyvHttpBuffer( struct HttpBuffer *b , char *format , va_list valist )
{
	int		len ;
	
	int		nret = 0 ;
	
	while(1)
	{
		len = VSNPRINTF( b->base , b->buf_size-1 , format , valist ) ;
		if( len == -1 || len == b->buf_size-1 )
		{
			nret = ReallocHttpBuffer( b , -1 ) ;
			if( nret )
				return nret;
		}
		else
		{
			b->fill_ptr = b->base + len ;
			break;
		}
	}
	
	return 0;
}

int StrcatHttpBuffer( struct HttpBuffer *b , char *str )
{
	long		len ;
	
	int		nret = 0 ;
	
	len = strlen(str) ;
	while( (b->fill_ptr-b->base) + len > b->buf_size-1 )
	{
		nret = ReallocHttpBuffer( b , -1 ) ;
		if( nret )
			return nret;
	}
	
	memcpy( b->fill_ptr , str , len );
	b->fill_ptr += len ;
	
	return 0;
}

int StrcatfHttpBuffer( struct HttpBuffer *b , char *format , ... )
{
	va_list		valist ;
	long		len ;
	
	int		nret = 0 ;
	
	while(1)
	{
		va_start( valist , format );
		len = VSNPRINTF( b->fill_ptr , b->buf_size-1 - (b->fill_ptr-b->base) , format , valist ) ;
		va_end( valist );
		if( len == -1 || len == b->buf_size-1 - (b->fill_ptr-b->base) )
		{
			nret = ReallocHttpBuffer( b , -1 ) ;
			if( nret )
				return nret;
		}
		else
		{
			b->fill_ptr += len ;
			break;
		}
	}
	
	return 0;
}

int StrcatvHttpBuffer( struct HttpBuffer *b , char *format , va_list valist )
{
	int		len ;
	
	int		nret = 0 ;
	
	while(1)
	{
		len = VSNPRINTF( b->fill_ptr , b->buf_size-1 - (b->fill_ptr-b->base) , format , valist ) ;
		if( len == -1 || len == b->buf_size-1 - (b->fill_ptr-b->base) )
		{
			nret = ReallocHttpBuffer( b , -1 ) ;
			if( nret )
				return nret;
		}
		else
		{
			b->fill_ptr += len ;
			break;
		}
	}
	
	return 0;
}

int MemcatHttpBuffer( struct HttpBuffer *b , char *base , int len )
{
	int		nret = 0 ;
	
	while( (b->fill_ptr-b->base) + len > b->buf_size-1 )
	{
		nret = ReallocHttpBuffer( b , -1 ) ;
		if( nret )
			return nret;
	}
	
	memcpy( b->fill_ptr , base , len );
	b->fill_ptr += len ;
	
	return 0;
}

int StrcatHttpBufferFromFile( struct HttpBuffer *b , char *pathfilename , int *p_filesize )
{
	int		filesize = -1 ;
	int		new_buf_size ;
	FILE		*fp = NULL ;
	
	int		nret = 0 ;
	
	if( p_filesize )
		filesize = (*p_filesize) ;
		
	if( filesize == -1 )
	{
		struct stat	st ;
		
		nret = stat( pathfilename , & st ) ;
		if( nret == -1 )
			return FASTERHTTP_ERROR_FILE_NOT_FOUND;
		filesize = st.st_size ;
	}
	
	if( filesize > 0 )
	{
		new_buf_size = (b->fill_ptr-b->base) + filesize + 1 ;
		if( new_buf_size > b->buf_size )
		{
			nret = ReallocHttpBuffer( b , new_buf_size ) ;
			if( nret )
				return nret;
		}
		
		fp = fopen( pathfilename , "rb" ) ;
		if( fp == NULL )
			return FASTERHTTP_ERROR_FILE_NOT_FOUND;
		
		nret = fread( b->fill_ptr , filesize , 1 , fp ) ;
		if( nret != 1 )
			return FASTERHTTP_ERROR_FILE_NOT_FOUND;
		
		fclose( fp );
		
		b->fill_ptr += filesize ;
	}
	
	if( p_filesize )
		(*p_filesize) = filesize ;
	
	return 0;
}

int InitHttpBuffer( struct HttpBuffer *b , int buf_size )
{
	b->buf_size = buf_size ;
	b->base = (char*)malloc( b->buf_size ) ;
	if( b->base == NULL )
	{
		return -1;
	}
	memset( b->base , 0x00 , sizeof(b->buf_size) );
	b->ref_flag = 0 ;
	b->fill_ptr = b->base ;
	b->process_ptr = b->base ;
	
	return 0;
}

int InitHttpBuffer2( struct HttpBuffer *b , int buf_size , char *base )
{
	b->buf_size = buf_size ;
	b->base = base ;
	b->ref_flag = 1 ;
	b->fill_ptr = b->base ;
	b->process_ptr = b->base ;
	
	return 0;
}

void ReformingHttpBuffer( struct HttpBuffer *b )
{
	int	len = b->fill_ptr - b->process_ptr ;
	memmove( b->base , b->process_ptr , len );
	b->process_ptr = b->base ;
	b->fill_ptr = b->base + len ;
	
	return;
}

void ResetHttpBuffer( struct HttpBuffer *b )
{
	b->fill_ptr = b->base ;
	b->process_ptr = b->base ;
	
	return;
}

void CleanHttpBuffer( struct HttpBuffer *b )
{
	if( b->ref_flag == 0 && b->buf_size > 0 && b->base )
	{
		free( b->base );
		b->base = NULL ;
		b->buf_size = 0 ;
	}
	
	return;
}

struct HttpBuffer *AllocHttpBuffer( int buf_size )
{
	struct HttpBuffer	*b = NULL ;
	
	int			nret = 0 ;
	
	b = (struct HttpBuffer *)malloc( sizeof(struct HttpBuffer) ) ;
	if( b == NULL )
		return NULL;
	memset( b , 0x00 , sizeof(struct HttpBuffer) );
	
	nret = InitHttpBuffer( b , buf_size ) ;
	if( nret )
		FreeHttpBuffer( b );
	
	return b;
}

struct HttpBuffer *AllocHttpBuffer2( int buf_size , char *base )
{
	struct HttpBuffer	*b = NULL ;
	
	int			nret = 0 ;
	
	b = (struct HttpBuffer *)malloc( sizeof(struct HttpBuffer) ) ;
	if( b == NULL )
		return NULL;
	memset( b , 0x00 , sizeof(struct HttpBuffer) );
	
	nret = InitHttpBuffer2( b , buf_size , base ) ;
	if( nret )
		FreeHttpBuffer( b );
	
	return b;
}

int ReallocHttpBuffer( struct HttpBuffer *b , int new_buf_size )
{
	char	*new_base = NULL ;
	int	fill_len = b->fill_ptr - b->base ;
	int	process_len = b->process_ptr - b->base ;
	
	if( b->ref_flag == 1 )
		return 1;
	
	if( new_buf_size == -1 )
	{
		if( b->buf_size <= 100*1024*1024 )
			new_buf_size = b->buf_size * 2 ;
		else
			new_buf_size += b->buf_size + 100*1024*1024 ;
	}
	new_base = (char *)realloc( b->base , new_buf_size ) ;
	if( new_base == NULL )
		return FASTERHTTP_ERROR_ALLOC;
	memset( new_base + fill_len , 0x00 , new_buf_size - fill_len );
	b->buf_size = new_buf_size ;
	b->base = new_base ;
	b->fill_ptr = b->base + fill_len ;
	b->process_ptr = b->base + process_len ;
	
	return 0;
}

void FreeHttpBuffer( struct HttpBuffer *b )
{
	if( b )
	{
		CleanHttpBuffer( b );
		
		free( b );
	}
	
	return;
}

void SetHttpBufferPtr( struct HttpBuffer *b , int buf_size , char *base )
{
	if( b->ref_flag == 0 && b->buf_size > 0 && b->base )
	{
		free( b->base );
	}
	
	b->buf_size = buf_size ;
	b->base = base ;
	b->ref_flag = 1 ;
	b->fill_ptr = b->base + buf_size - 1 ;
	b->process_ptr = b->base ;
	
	return;
}

int DuplicateHttpBufferPtr( struct HttpBuffer *b )
{
	char	*base = NULL ;
	
	if( b->base == NULL )
		return 1;
	
	base = (char*)malloc( b->buf_size ) ;
	if( base == NULL )
		return -1;
	memcpy( base , b->base , b->buf_size );
	b->ref_flag = 0 ;
	b->fill_ptr = base + ( b->fill_ptr-b->base ) ;
	b->process_ptr = base + ( b->process_ptr-b->base ) ;
	
	b->base = base ;
	
	return 0;
}

void AppendHttpBuffer( struct HttpEnv *e , struct HttpBuffer *b )
{
	e->p_append_buffer = b ;
	return;
}

void OffsetHttpBufferFillPtr( struct HttpBuffer *b , int offset )
{
	b->fill_ptr += offset ;
	return;
}

int GetHttpBufferLengthFilled( struct HttpBuffer *b )
{
	if( b )
		return b->fill_ptr-b->base;
	else
		return 0;
}

int GetHttpBufferLengthUnfilled( struct HttpBuffer *b )
{
	if( b )
		return b->buf_size-1-(b->fill_ptr-b->base);
	else
		return 0;
}

void OffsetHttpBufferProcessPtr( struct HttpBuffer *b , int offset )
{
	b->process_ptr += offset ;
	return;
}

int GetHttpBufferLengthProcessed( struct HttpBuffer *b )
{
	if( b )
		return b->process_ptr-b->base;
	else
		return 0;
}

int GetHttpBufferLengthUnprocessed( struct HttpBuffer *b )
{
	if( b )
		return b->fill_ptr-b->process_ptr;
	else
		return 0;
}

void CopyHttpHeader_STATUSCODE( struct HttpEnv *e , struct HttpEnv *e2 )
{
	memmove( & (e->headers.STATUSCODE) , & (e2->headers.STATUSCODE) , sizeof(struct HttpHeader) );
	return;
}

#ifndef STAT
#if ( defined __linux__ ) || ( defined __unix ) || ( defined _AIX )
#define STAT		stat
#define FSTAT		fstat
#elif ( defined _WIN32 )
#define STAT		_stat
#define FSTAT		_fstat
#endif
#endif

int SplitHttpUri( char *wwwroot , char *uri , int uri_len , struct HttpUri *p_httpuri )
{
	char		*uri_end = uri + uri_len ;
	char		*p_slash ;
	char		*p_dot ;
	char		*p_question_mark ;
	
	char		pathfilename[ 256 + 1 ] ;
	struct STAT	st ;
	
	char		*p = NULL ;
	
	int		nret = 0 ;
	
	for( p = uri_end - 1 ; p >= uri ; p-- )
	{
		if( (*p) == '/' )
			break;
	}
	if( p >= uri )
	{
		p_slash = p ;
		p++;
	}
	else
	{
		p_slash = NULL ;
		p = uri ;
	}
	
	p_dot = NULL ;
	p_question_mark = NULL ;
	for( ; p < uri_end ; p++ )
	{
		if( (*p) == '.' && p_question_mark == NULL )
			p_dot = p ;
		else if( (*p) == '?' )
			p_question_mark = p ;
	}
	
	if( p_slash == NULL )
	{
		if( p_dot == NULL )
		{
			if( p_question_mark == NULL )
			{
				/* "mydir" */
				if( wwwroot == NULL )
				{
					return 0001;
				}
				else
				{
					memset( pathfilename , 0x00 , sizeof(pathfilename) );
					SNPRINTF( pathfilename , sizeof(pathfilename) , "%s/%.*s" , wwwroot , uri_len , uri );
#if ( defined _WIN32 )
					if( pathfilename[strlen(pathfilename)-1] == '/' || pathfilename[strlen(pathfilename)-1] == '\\' )
						pathfilename[strlen(pathfilename)-1] = '\0' ;
#endif
					nret = STAT( pathfilename , & st ) ;
					if( nret == -1 )
					{
						return -0001;
					}
					else if( STAT_DIRECTORY(st) )
					{
						p_httpuri->dirname_base = uri ;
						p_httpuri->dirname_len = uri_len ;
						p_httpuri->filename_base = uri ;
						p_httpuri->filename_len = 0 ;
						p_httpuri->main_filename_base = uri ;
						p_httpuri->main_filename_len = 0 ;
						p_httpuri->ext_filename_base = uri_end ;
						p_httpuri->ext_filename_len = 0 ;
						p_httpuri->param_base = uri_end ;
						p_httpuri->param_len = 0 ;
					}
					else
					{
						p_httpuri->dirname_base = uri ;
						p_httpuri->dirname_len = 0 ;
						p_httpuri->filename_base = uri ;
						p_httpuri->filename_len = uri_len ;
						p_httpuri->main_filename_base = uri ;
						p_httpuri->main_filename_len = uri_len ;
						p_httpuri->ext_filename_base = uri_end ;
						p_httpuri->ext_filename_len = 0 ;
						p_httpuri->param_base = uri_end ;
						p_httpuri->param_len = 0 ;
					}
				}
			}
			else
			{
				/* "mydir?id=123" */
				memset( pathfilename , 0x00 , sizeof(pathfilename) );
				SNPRINTF( pathfilename , sizeof(pathfilename) , "%s/%.*s" , wwwroot , (int)(p_question_mark-uri) , uri );
#if ( defined _WIN32 )
					if( pathfilename[strlen(pathfilename)-1] == '/' || pathfilename[strlen(pathfilename)-1] == '\\' )
						pathfilename[strlen(pathfilename)-1] = '\0' ;
#endif
				nret = STAT( pathfilename , & st ) ;
				if( nret == -1 )
				{
					return -0011;
				}
				else if( STAT_DIRECTORY(st) )
				{
					p_httpuri->dirname_base = uri ;
					p_httpuri->dirname_len = p_question_mark - uri ;
					p_httpuri->filename_base = uri ;
					p_httpuri->filename_len = 0 ;
					p_httpuri->main_filename_base = uri ;
					p_httpuri->main_filename_len = 0 ;
					p_httpuri->ext_filename_base = p_question_mark ;
					p_httpuri->ext_filename_len = 0 ;
					p_httpuri->param_base = p_question_mark + 1 ;
					p_httpuri->param_len = uri_end - (p_question_mark+1) ;
				}
				else
				{
					p_httpuri->dirname_base = uri ;
					p_httpuri->dirname_len = 0 ;
					p_httpuri->filename_base = uri ;
					p_httpuri->filename_len = p_question_mark - uri ;
					p_httpuri->main_filename_base = uri ;
					p_httpuri->main_filename_len = p_question_mark - uri ;
					p_httpuri->ext_filename_base = p_question_mark ;
					p_httpuri->ext_filename_len = 0 ;
					p_httpuri->param_base = p_question_mark + 1 ;
					p_httpuri->param_len = uri_end - (p_question_mark+1) ;
				}
			}
		}
		else
		{
			if( p_question_mark == NULL )
			{
				/* "index.html" */
				p_httpuri->dirname_base = uri ;
				p_httpuri->dirname_len = 0 ;
				p_httpuri->filename_base = uri ;
				p_httpuri->filename_len = uri_len ;
				p_httpuri->main_filename_base = uri ;
				p_httpuri->main_filename_len = p_dot - uri ;
				p_httpuri->ext_filename_base = p_dot + 1 ;
				p_httpuri->ext_filename_len = uri_end - (p_dot+1) ;
				p_httpuri->param_base = uri_end ;
				p_httpuri->param_len = 0 ;
			}
			else
			{
				/* "index.html?id=123" */
				p_httpuri->dirname_base = uri ;
				p_httpuri->dirname_len = 0 ;
				p_httpuri->filename_base = uri ;
				p_httpuri->filename_len = p_question_mark - uri ;
				p_httpuri->main_filename_base = uri ;
				p_httpuri->main_filename_len = p_dot - uri ;
				p_httpuri->ext_filename_base = p_dot + 1 ;
				p_httpuri->ext_filename_len = p_question_mark - (p_dot+1) ;
				p_httpuri->param_base = p_question_mark + 1 ;
				p_httpuri->param_len = uri_end - (p_question_mark+1) ;
			}
		}
	}
	else
	{
		if( p_dot == NULL )
		{
			if( p_question_mark == NULL )
			{
				/* "/mydir" */
				if( wwwroot == NULL )
				{
					return 1001;
				}
				else
				{
					memset( pathfilename , 0x00 , sizeof(pathfilename) );
					SNPRINTF( pathfilename , sizeof(pathfilename) , "%s%.*s" , wwwroot , uri_len , uri );
#if ( defined _WIN32 )
					if( pathfilename[strlen(pathfilename)-1] == '/' || pathfilename[strlen(pathfilename)-1] == '\\' )
						pathfilename[strlen(pathfilename)-1] = '\0' ;
#endif
					nret = STAT( pathfilename , & st ) ;
					if( nret == -1 )
					{
						return -1001;
					}
					else if( STAT_DIRECTORY(st) )
					{
						p_httpuri->dirname_base = uri ;
						p_httpuri->dirname_len = uri_len ;
						p_httpuri->filename_base = uri ;
						p_httpuri->filename_len = 0 ;
						p_httpuri->main_filename_base = uri ;
						p_httpuri->main_filename_len = 0 ;
						p_httpuri->ext_filename_base = uri_end ;
						p_httpuri->ext_filename_len = 0 ;
						p_httpuri->param_base = uri_end ;
						p_httpuri->param_len = 0 ;
					}
					else
					{
						p_httpuri->dirname_base = uri ;
						p_httpuri->dirname_len = p_slash - uri ;
						p_httpuri->filename_base = p_slash + 1 ;
						p_httpuri->filename_len = uri_end - (p_slash+1) ;
						p_httpuri->main_filename_base = p_slash + 1 ;
						p_httpuri->main_filename_len = uri_end - (p_slash+1) ;
						p_httpuri->ext_filename_base = p_slash + 1 ;
						p_httpuri->ext_filename_len = 0 ;
						p_httpuri->param_base = p_slash + 1 ;
						p_httpuri->param_len = 0 ;
					}
				}
			}
			else
			{
				/* "/mydir?id=123" */
				memset( pathfilename , 0x00 , sizeof(pathfilename) );
				SNPRINTF( pathfilename , sizeof(pathfilename) , "%s%.*s" , wwwroot , (int)(p_question_mark-uri) , uri );
#if ( defined _WIN32 )
					if( pathfilename[strlen(pathfilename)-1] == '/' || pathfilename[strlen(pathfilename)-1] == '\\' )
						pathfilename[strlen(pathfilename)-1] = '\0' ;
#endif
				nret = STAT( pathfilename , & st ) ;
				if( nret == -1 )
				{
					return -1011;
				}
				else if( STAT_DIRECTORY(st) )
				{
					p_httpuri->dirname_base = uri ;
					p_httpuri->dirname_len = p_question_mark - uri ;
					p_httpuri->filename_base = uri ;
					p_httpuri->filename_len = 0 ;
					p_httpuri->main_filename_base = p_question_mark ;
					p_httpuri->main_filename_len = 0 ;
					p_httpuri->ext_filename_base = p_question_mark ;
					p_httpuri->ext_filename_len = 0 ;
					p_httpuri->param_base = p_question_mark + 1 ;
					p_httpuri->param_len = uri_end - (p_question_mark+1) ;
				}
				else
				{
					p_httpuri->dirname_base = uri ;
					p_httpuri->dirname_len = p_slash - uri ;
					p_httpuri->filename_base = p_slash + 1 ;
					p_httpuri->filename_len = p_question_mark - (p_slash+1) ;
					p_httpuri->main_filename_base = p_slash + 1 ;
					p_httpuri->main_filename_len = p_question_mark - (p_slash+1) ;
					p_httpuri->ext_filename_base = p_slash + 1 ;
					p_httpuri->ext_filename_len = 0 ;
					p_httpuri->param_base = p_question_mark + 1 ;
					p_httpuri->param_len = uri_end - (p_question_mark+1) ;
				}
			}
		}
		else
		{
			if( p_question_mark == NULL )
			{
				/* "/mydir/index.html" */
				p_httpuri->dirname_base = uri ;
				p_httpuri->dirname_len = p_slash - uri ;
				p_httpuri->filename_base = p_slash + 1 ;
				p_httpuri->filename_len = uri_end - p_slash ;
				p_httpuri->main_filename_base = p_slash + 1 ;
				p_httpuri->main_filename_len = p_dot - (p_slash+1) ;
				p_httpuri->ext_filename_base = p_dot + 1 ;
				p_httpuri->ext_filename_len = uri_end - (p_dot+1) ;
				p_httpuri->param_base = uri ;
				p_httpuri->param_len = 0 ;
			}
			else
			{
				/* "/mydir/index.html?id=123" */
				p_httpuri->dirname_base = uri ;
				p_httpuri->dirname_len = p_slash - uri ;
				p_httpuri->filename_base = p_slash + 1 ;
				p_httpuri->filename_len = p_question_mark - (p_slash+1) ;
				p_httpuri->main_filename_base = p_slash + 1 ;
				p_httpuri->main_filename_len = p_dot - (p_slash+1) ;
				p_httpuri->ext_filename_base = p_dot + 1 ;
				p_httpuri->ext_filename_len = p_question_mark - (p_dot+1) ;
				p_httpuri->param_base = p_question_mark + 1 ;
				p_httpuri->param_len = uri_end - (p_question_mark+1) ;
			}
		}
	}
	
	return 0;
}

