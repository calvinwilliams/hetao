/*
 * fasterhttp
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#ifndef _H_FASTERHTTP_
#define _H_FASTERHTTP_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

#if ( defined _WIN32 )
#include <winsock2.h>
#include <windows.h>
#elif ( defined __unix ) || ( defined __linux__ )
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#endif

char *strcasestr(const char *haystack, const char *needle);

#include <openssl/ssl.h>

#include <zlib.h>

#if ( defined _WIN32 )
#ifndef _WINDLL_FUNC
#define _WINDLL_FUNC            _declspec(dllexport)
#endif
#elif ( defined __unix ) || ( defined __linux__ )
#ifndef _WINDLL_FUNC
#define _WINDLL_FUNC		extern
#endif
#endif

#ifndef LIKELY
#if __GNUC__ >= 3
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif
#endif

#ifndef STRCMP
#define STRCMP(_a_,_C_,_b_) ( strcmp(_a_,_b_) _C_ 0 )
#define STRNCMP(_a_,_C_,_b_,_n_) ( strncmp(_a_,_b_,_n_) _C_ 0 )
#endif

#ifndef STRICMP
#if ( defined _WIN32 )
#define STRICMP(_a_,_C_,_b_) ( stricmp(_a_,_b_) _C_ 0 )
#define STRNICMP(_a_,_C_,_b_,_n_) ( strnicmp(_a_,_b_,_n_) _C_ 0 )
#elif ( defined __unix ) || ( defined _AIX ) || ( defined __linux__ )
#define STRICMP(_a_,_C_,_b_) ( strcasecmp(_a_,_b_) _C_ 0 )
#define STRNICMP(_a_,_C_,_b_,_n_) ( strncasecmp(_a_,_b_,_n_) _C_ 0 )
#endif
#endif

#ifndef MEMCMP
#define MEMCMP(_a_,_C_,_b_,_n_) ( memcmp(_a_,_b_,_n_) _C_ 0 )
#endif

#ifndef STRISTR
#if ( defined _WIN32 )
#define STRISTR		strstr /* ÒÔºó¸Ä */
#elif ( defined __unix ) || ( defined _AIX ) || ( defined __linux__ )
#define STRISTR		strcasestr
#endif
#endif

#ifndef SNPRINTF
#if ( defined _WIN32 )
#define SNPRINTF        _snprintf
#define VSNPRINTF       _vsnprintf
#elif ( defined __unix ) || ( defined _AIX ) || ( defined __linux__ )
#define SNPRINTF        snprintf
#define VSNPRINTF       vsnprintf
#endif
#endif

#if ( defined _WIN32 )
#define SOCKLEN_T	int
#define CLOSESOCKET	closesocket
#elif ( defined __unix ) || ( defined __linux__ )
#define SOCKET		int
#define SOCKLEN_T	socklen_t
#define CLOSESOCKET	close
#endif

#if ( defined _WIN32 )
#define STAT_DIRECTORY(_st_) ((_st_.st_mode)&_S_IFDIR)
#elif ( defined __unix ) || ( defined __linux__ )
#define STAT_DIRECTORY(_st_) S_ISDIR(_st_.st_mode)
#endif

#define HTTP_CONTINUE				100 
#define HTTP_SWITCHING_PROTOCOL			101 
#define HTTP_OK					200 
#define HTTP_CREATED				201 
#define HTTP_ACCEPTED				202 
#define HTTP_NON_AUTHORITATIVE_INFORMATION	203 
#define HTTP_NO_CONTENT				204 
#define HTTP_RESET_CONTENT			205 
#define HTTP_PARTIAL_CONTENT			206 
#define HTTP_MULTIPLE_CHOICES			300 
#define HTTP_MOVED_PERMANNETLY			301 
#define HTTP_FOUND				302 
#define HTTP_SEE_OTHER				303 
#define HTTP_NOT_MODIFIED			304 
#define HTTP_USE_PROXY				305 
#define HTTP_TEMPORARY_REDIRECT			307 
#define HTTP_BAD_REQUEST			400 
#define HTTP_UNAUTHORIZED			401 
#define HTTP_PAYMENT_REQUIRED			402 
#define HTTP_FORBIDDEN				403 
#define HTTP_NOT_FOUND				404 
#define HTTP_METHOD_NOT_ALLOWED			405 
#define HTTP_NOT_ACCEPTABLE			406 
#define HTTP_PROXY_AUTHENTICATION_REQUIRED	407 
#define HTTP_REQUEST_TIMEOUT			408 
#define HTTP_CONFLICT				409 
#define HTTP_GONE				410 
#define HTTP_LENGTH_REQUIRED			411 
#define HTTP_PRECONDITION_FAILED		412 
#define HTTP_REQUEST_ENTITY_TOO_LARGE		413 
#define HTTP_URI_TOO_LONG			414 
#define HTTP_UNSUPPORTED_MEDIA_TYPE		415 
#define HTTP_REQUESTED_RANGE_NOT_SATISFIABLE	416 
#define HTTP_EXPECTATION_FAILED			417 
#define HTTP_INTERNAL_SERVER_ERROR		500 
#define HTTP_NOT_IMPLEMENTED			501 
#define HTTP_BAD_GATEWAY			502 
#define HTTP_SERVICE_UNAVAILABLE		503 
#define HTTP_GATEWAY_TIMEOUT			504 
#define HTTP_VERSION_NOT_SUPPORTED		505

#define HTTP_CONTINUE_S				"100"
#define HTTP_SWITCHING_PROTOCOL_S		"101"
#define HTTP_OK_S				"200"
#define HTTP_CREATED_S				"201"
#define HTTP_ACCEPTED_S				"202"
#define HTTP_NON_AUTHORITATIVE_INFORMATION_S	"203"
#define HTTP_NO_CONTENT_S			"204"
#define HTTP_RESET_CONTENT_S			"205"
#define HTTP_PARTIAL_CONTENT_S			"206"
#define HTTP_MULTIPLE_CHOICES_S			"300"
#define HTTP_MOVED_PERMANNETLY_S		"301"
#define HTTP_FOUND_S				"302"
#define HTTP_SEE_OTHER_S			"303"
#define HTTP_NOT_MODIFIED_S			"304"
#define HTTP_USE_PROXY_S			"305"
#define HTTP_TEMPORARY_REDIRECT_S		"307"
#define HTTP_BAD_REQUEST_S			"400"
#define HTTP_UNAUTHORIZED_S			"401"
#define HTTP_PAYMENT_REQUIRED_S			"402"
#define HTTP_FORBIDDEN_S			"403"
#define HTTP_NOT_FOUND_S			"404"
#define HTTP_METHOD_NOT_ALLOWED_S		"405"
#define HTTP_NOT_ACCEPTABLE_S			"406"
#define HTTP_PROXY_AUTHENTICATION_REQUIRED_S	"407"
#define HTTP_REQUEST_TIMEOUT_S			"408"
#define HTTP_CONFLICT_S				"409"
#define HTTP_GONE_S				"410"
#define HTTP_LENGTH_REQUIRED_S			"411"
#define HTTP_PRECONDITION_FAILED_S		"412"
#define HTTP_REQUEST_ENTITY_TOO_LARGE_S		"413"
#define HTTP_URI_TOO_LONG_S			"414"
#define HTTP_UNSUPPORTED_MEDIA_TYPE_S		"415"
#define HTTP_REQUESTED_RANGE_NOT_SATISFIABLE_S	"416"
#define HTTP_EXPECTATION_FAILED_S		"417"
#define HTTP_INTERNAL_SERVER_ERROR_S		"500"
#define HTTP_NOT_IMPLEMENTED_S			"501"
#define HTTP_BAD_GATEWAY_S			"502"
#define HTTP_SERVICE_UNAVAILABLE_S		"503"
#define HTTP_GATEWAY_TIMEOUT_S			"504"
#define HTTP_VERSION_NOT_SUPPORTED_S		"505"

#define HTTP_CONTINUE_T				"Continue"
#define HTTP_SWITCHING_PROTOCOL_T		"Switching Protocol"
#define HTTP_OK_T				"OK"
#define HTTP_CREATED_T				"Created"
#define HTTP_ACCEPTED_T				"Accepted"
#define HTTP_NON_AUTHORITATIVE_INFORMATION_T	"Non Authoritative Information"
#define HTTP_NO_CONTENT_T			"No Content"
#define HTTP_RESET_CONTENT_T			"Reset Content"
#define HTTP_PARTIAL_CONTENT_T			"Partial Content"
#define HTTP_MULTIPLE_CHOICES_T			"Multiple Choices"
#define HTTP_MOVED_PERMANNETLY_T		"Moved Permannetly"
#define HTTP_FOUND_T				"Found"
#define HTTP_SEE_OTHER_T			"See Other"
#define HTTP_NOT_MODIFIED_T			"Not Modified"
#define HTTP_USE_PROXY_T			"Use Proxy"
#define HTTP_TEMPORARY_REDIRECT_T		"Temporary Redirect"
#define HTTP_BAD_REQUEST_T			"Bad Request"
#define HTTP_UNAUTHORIZED_T			"Unauthorized"
#define HTTP_PAYMENT_REQUIRED_T			"Payment Required"
#define HTTP_FORBIDDEN_T			"Forbidden"
#define HTTP_NOT_FOUND_T			"Not Found"
#define HTTP_METHOD_NOT_ALLOWED_T		"Method Not Allowed"
#define HTTP_NOT_ACCEPTABLE_T			"Not Acceptable"
#define HTTP_PROXY_AUTHENTICATION_REQUIRED_T	"Proxy Authentication Required"
#define HTTP_REQUEST_TIMEOUT_T			"Request Timeout"
#define HTTP_CONFLICT_T				"Conflict"
#define HTTP_GONE_T				"Gone"
#define HTTP_LENGTH_REQUIRED_T			"Length Request"
#define HTTP_PRECONDITION_FAILED_T		"Precondition Failed"
#define HTTP_REQUEST_ENTITY_TOO_LARGE_T		"Request Entity Too Large"
#define HTTP_URI_TOO_LONG_T			"Request URI Too Long"
#define HTTP_UNSUPPORTED_MEDIA_TYPE_T		"Unsupported Media Type"
#define HTTP_REQUESTED_RANGE_NOT_SATISFIABLE_T	"Requested Range Not Satisfiable"
#define HTTP_EXPECTATION_FAILED_T		"Expectation Failed"
#define HTTP_INTERNAL_SERVER_ERROR_T		"Internal Server Error"
#define HTTP_NOT_IMPLEMENTED_T			"Not Implemented"
#define HTTP_BAD_GATEWAY_T			"Bad Gateway"
#define HTTP_SERVICE_UNAVAILABLE_T		"Service Unavailable"
#define HTTP_GATEWAY_TIMEOUT_T			"Gateway Timeout"
#define HTTP_VERSION_NOT_SUPPORTED_T		"HTTP Version Not Supported"

#define FASTERHTTP_ERROR_ALLOC				(-HTTP_INTERNAL_SERVER_ERROR*100)-11
#define FASTERHTTP_ERROR_PARAMTER			(-HTTP_INTERNAL_SERVER_ERROR*100)-12
#define FASTERHTTP_ERROR_INTERNAL			(-HTTP_INTERNAL_SERVER_ERROR*100)-14
#define FASTERHTTP_ERROR_FILE_NOT_FOUND			(-HTTP_NOT_FOUND*100)
#define FASTERHTTP_ERROR_TCP_SELECT_RECEIVE		(-HTTP_BAD_REQUEST*100)-31
#define FASTERHTTP_ERROR_TCP_SELECT_RECEIVE_TIMEOUT	(-HTTP_REQUEST_TIMEOUT*100)-32
#define FASTERHTTP_ERROR_TCP_RECEIVE			(-HTTP_BAD_REQUEST*100)-33
#define FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK		10
#define FASTERHTTP_ERROR_TCP_SELECT_SEND		(-HTTP_BAD_REQUEST*100)-41
#define FASTERHTTP_ERROR_TCP_SELECT_SEND_TIMEOUT	(-HTTP_REQUEST_TIMEOUT*100)-42
#define FASTERHTTP_ERROR_TCP_SEND			(-HTTP_BAD_REQUEST*100)-43
#define FASTERHTTP_ERROR_TCP_CLOSE			-200
#define FASTERHTTP_INFO_TCP_CLOSE			200
#define FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER		100
#define FASTERHTTP_ERROR_METHOD_INVALID			(-HTTP_NOT_IMPLEMENTED*100)
#define FASTERHTTP_ERROR_VERSION_NOT_SUPPORTED		(-HTTP_VERSION_NOT_SUPPORTED*100)
#define FASTERHTTP_ERROR_HTTP_HEADERSTARTLINE_INVALID	(-HTTP_BAD_REQUEST*100)-51
#define FASTERHTTP_ERROR_HTTP_HEADER_INVALID		(-HTTP_BAD_REQUEST*100)-52
#define FASTERHTTP_ERROR_HTTP_TRUNCATE			(-HTTP_BAD_REQUEST*100)-61
#define FASTERHTTP_ERROR_ZLIB__				(-HTTP_INTERNAL_SERVER_ERROR*100)

#define FASTERHTTP_TIMEOUT_DEFAULT			60

#define FASTERHTTP_REQUEST_BUFSIZE_DEFAULT		1*1024
#define FASTERHTTP_RESPONSE_BUFSIZE_DEFAULT		4*1024
#define FASTERHTTP_HEADER_ARRAYSIZE_DEFAULT		16

#define FASTERHTTP_REQUEST_BUFSIZE_MAX			1024*1024
#define FASTERHTTP_RESPONSE_BUFSIZE_MAX			1024*1024
#define FASTERHTTP_HEADER_ARRAYSIZE_MAX			64

#define FASTERHTTP_PARSESTEP_BEGIN				0
#define FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_METHOD0		110
#define FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_METHOD		111
#define FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_URI0		120
#define FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_URI		121
#define FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_VERSION0		130
#define FASTERHTTP_PARSESTEP_REQUESTSTARTLINE_VERSION		131
#define FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_VERSION0		150
#define FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_VERSION		151
#define FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_STATUSCODE0	160
#define FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_STATUSCODE	161
#define FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_REASONPHRASE0	170
#define FASTERHTTP_PARSESTEP_RESPONSESTARTLINE_REASONPHRASE	171
#define FASTERHTTP_PARSESTEP_HEADER_NAME0			210
#define FASTERHTTP_PARSESTEP_HEADER_NAME			211
#define FASTERHTTP_PARSESTEP_HEADER_VALUE0			220
#define FASTERHTTP_PARSESTEP_HEADER_VALUE			221
#define FASTERHTTP_PARSESTEP_BODY				300
#define FASTERHTTP_PARSESTEP_CHUNKED_SIZE			311
#define FASTERHTTP_PARSESTEP_CHUNKED_DATA			312
#define FASTERHTTP_PARSESTEP_DONE				400

#define HTTP_RETURN					'\r'
#define HTTP_NEWLINE					'\n'
#define HTTP_RETURN_NEWLINE				"\r\n"

#define HTTP_HEADER_METHOD				"METHOD"
#define HTTP_HEADER_URI					"URI"
#define HTTP_HEADER_VERSION				"VERSION"
#define HTTP_HEADER_STATUSCODE				"STATUSCODE"
#define HTTP_HEADER_REASONPHRASE			"REASONPHRASE"

#define HTTP_METHOD_GET					"GET"
#define HTTP_METHOD_POST				"POST"
#define HTTP_METHOD_HEAD				"HEAD"
#define HTTP_METHOD_TRACE				"TRACE"
#define HTTP_METHOD_OPTIONS				"OPTIONS"
#define HTTP_METHOD_PUT					"PUT"
#define HTTP_METHOD_DELETE				"DELETE"

#define HTTP_METHOD_GET_N				1
#define HTTP_METHOD_POST_N				2
#define HTTP_METHOD_HEAD_N				3
#define HTTP_METHOD_TRACE_N				4
#define HTTP_METHOD_OPTIONS_N				5
#define HTTP_METHOD_PUT_N				6
#define HTTP_METHOD_DELETE_N				7

#define HTTP_VERSION_1_0				"HTTP/1.0"
#define HTTP_VERSION_1_1				"HTTP/1.1"
#define HTTP_VERSION_1_0_N				10
#define HTTP_VERSION_1_1_N				11

#define HTTP_HEADER_CONTENT_LENGTH			"Content-Length"
#define HTTP_HEADER_TRANSFERENCODING			"Transfer-Encoding"
#define HTTP_HEADER_TRANSFERENCODING__CHUNKED		"chunked"
#define HTTP_HEADER_TRAILER				"Trailer"
#define HTTP_HEADER_CONTENTENCODING			"Content-Encoding"
#define HTTP_HEADER_ACCEPTENCODING			"Accept-Encoding"
#define HTTP_HEADER_CONNECTION				"Connection"
#define HTTP_HEADER_CONNECTION__KEEPALIVE		"Keep-Alive"
#define HTTP_HEADER_CONNECTION__CLOSE			"Close"

#define HTTP_COMPRESSALGORITHM_GZIP			MAX_WBITS+16
#define HTTP_COMPRESSALGORITHM_DEFLATE			MAX_WBITS

struct HttpBuffer ;
struct HttpEnv ;

/* http env */
_WINDLL_FUNC struct HttpEnv *CreateHttpEnv();
_WINDLL_FUNC void ResetHttpEnv( struct HttpEnv *e );
_WINDLL_FUNC void DestroyHttpEnv( struct HttpEnv *e );

/* properties */
_WINDLL_FUNC void SetHttpTimeout( struct HttpEnv *e , long timeout );
_WINDLL_FUNC struct timeval *GetHttpElapse( struct HttpEnv *e );
_WINDLL_FUNC void EnableHttpResponseCompressing( struct HttpEnv *e , int enable_response_compressing );

_WINDLL_FUNC void SetParserCustomIntData( struct HttpEnv *e , int i );
_WINDLL_FUNC int GetParserCustomIntData( struct HttpEnv *e );
_WINDLL_FUNC void SetParserCustomPtrData( struct HttpEnv *e , void *ptr );
_WINDLL_FUNC void *GetParserCustomPtrData( struct HttpEnv *e );

/* global properties */
_WINDLL_FUNC void ResetAllHttpStatus();
_WINDLL_FUNC void SetHttpStatus( int status_code , char *status_code_s , char *status_text );

/* http client advance api */
_WINDLL_FUNC int RequestHttp( SOCKET sock , SSL *ssl , struct HttpEnv *e );

/* http server advance api */
typedef int funcProcessHttpRequest( struct HttpEnv *e , void *p );
_WINDLL_FUNC int ResponseAllHttp( SOCKET sock , SSL *ssl , struct HttpEnv *e , funcProcessHttpRequest *pfuncProcessHttpRequest , void *p );

/* http client api */
_WINDLL_FUNC int SendHttpRequest( SOCKET sock , SSL *ssl , struct HttpEnv *e );
_WINDLL_FUNC int ReceiveHttpResponse( SOCKET sock , SSL *ssl , struct HttpEnv *e );

/* http server api */
_WINDLL_FUNC int ReceiveHttpRequest( SOCKET sock , SSL *ssl , struct HttpEnv *e );
_WINDLL_FUNC int FormatHttpResponseStartLine( int status_code , struct HttpEnv *e , int fill_body_with_status_flag );
_WINDLL_FUNC int SendHttpResponse( SOCKET sock , SSL *ssl , struct HttpEnv *e );
_WINDLL_FUNC int CheckHttpKeepAlive( struct HttpEnv *e );

/* http client api with nonblock */
_WINDLL_FUNC int SendHttpRequestNonblock( SOCKET sock , SSL *ssl , struct HttpEnv *e );
_WINDLL_FUNC int ReceiveHttpResponseNonblock( SOCKET sock , SSL *ssl , struct HttpEnv *e );

/* http server api with nonblock */
_WINDLL_FUNC int ReceiveHttpRequestNonblock( SOCKET sock , SSL *ssl , struct HttpEnv *e );
_WINDLL_FUNC int SendHttpResponseNonblock( SOCKET sock , SSL *ssl , struct HttpEnv *e );

/* http test api */
_WINDLL_FUNC int ParseHttpResponse( struct HttpEnv *e );
_WINDLL_FUNC int ParseHttpRequest( struct HttpEnv *e );

/* http header and body */
_WINDLL_FUNC char *GetHttpHeaderPtr_METHOD( struct HttpEnv *e , int *p_value_len );
_WINDLL_FUNC int GetHttpHeaderLen_METHOD( struct HttpEnv *e );
_WINDLL_FUNC char *GetHttpHeaderPtr_URI( struct HttpEnv *e , int *p_value_len );
_WINDLL_FUNC int GetHttpHeaderLen_URI( struct HttpEnv *e );
_WINDLL_FUNC char *GetHttpHeaderPtr_VERSION( struct HttpEnv *e , int *p_value_len );
_WINDLL_FUNC int GetHttpHeaderLen_VERSION( struct HttpEnv *e );
_WINDLL_FUNC char *GetHttpHeaderPtr_STATUSCODE( struct HttpEnv *e , int *p_value_len );
_WINDLL_FUNC int GetHttpHeaderLen_STATUSCODE( struct HttpEnv *e );
_WINDLL_FUNC char *GetHttpHeaderPtr_REASONPHRASE( struct HttpEnv *e , int *p_value_len );
_WINDLL_FUNC int GetHttpHeaderLen_REASONPHRASE( struct HttpEnv *e );

_WINDLL_FUNC char GetHttpHeader_METHOD( struct HttpEnv *e );
_WINDLL_FUNC char GetHttpHeader_VERSION( struct HttpEnv *e );

_WINDLL_FUNC char *QueryHttpHeaderPtr( struct HttpEnv *e , char *name , int *p_value_len );
_WINDLL_FUNC int QueryHttpHeaderLen( struct HttpEnv *e , char *name );
_WINDLL_FUNC int CountHttpHeaders( struct HttpEnv *e );
_WINDLL_FUNC struct HttpHeader *TravelHttpHeaderPtr( struct HttpEnv *e , struct HttpHeader *p_header );
_WINDLL_FUNC char *GetHttpHeaderNamePtr( struct HttpHeader *p_header , int *p_name_len );
_WINDLL_FUNC int GetHttpHeaderNameLen( struct HttpHeader *p_header );
_WINDLL_FUNC char *GetHttpHeaderValuePtr( struct HttpHeader *p_header , int *p_value_len );
_WINDLL_FUNC int GetHttpHeaderValueLen( struct HttpHeader *p_header );

_WINDLL_FUNC char *GetHttpBodyPtr( struct HttpEnv *e , int *p_body_len );
_WINDLL_FUNC int GetHttpBodyLen( struct HttpEnv *e );

/* buffer operations */
_WINDLL_FUNC struct HttpBuffer *GetHttpRequestBuffer( struct HttpEnv *e );
_WINDLL_FUNC struct HttpBuffer *GetHttpResponseBuffer( struct HttpEnv *e );

_WINDLL_FUNC char *GetHttpBufferBase( struct HttpBuffer *b , int *p_data_len );
_WINDLL_FUNC int GetHttpBufferLength( struct HttpBuffer *b );

_WINDLL_FUNC int StrcpyHttpBuffer( struct HttpBuffer *b , char *str );
_WINDLL_FUNC int StrcpyfHttpBuffer( struct HttpBuffer *b , char *format , ... );
_WINDLL_FUNC int StrcpyvHttpBuffer( struct HttpBuffer *b , char *format , va_list valist );
_WINDLL_FUNC int StrcatHttpBuffer( struct HttpBuffer *b , char *str );
_WINDLL_FUNC int StrcatfHttpBuffer( struct HttpBuffer *b , char *format , ... );
_WINDLL_FUNC int StrcatvHttpBuffer( struct HttpBuffer *b , char *format , va_list valist );
_WINDLL_FUNC int MemcatHttpBuffer( struct HttpBuffer *b , char *base , int len );

_WINDLL_FUNC int StrcatHttpBufferFromFile( struct HttpBuffer *b , char *pathfilename , int *p_filesize );

/* util */
struct HttpUri
{
	char				*dirname_base ;
	int				dirname_len ;
	char				*filename_base ;
	int				filename_len ;
	char				*main_filename_base ;
	int				main_filename_len ;
	char				*ext_filename_base ;
	int				ext_filename_len ;
	char				*param_base ;
	int				param_len ;
} ;

_WINDLL_FUNC int SplitHttpUri( char *wwwroot , char *uri , int uri_len , struct HttpUri *p_httpuri );

#define SetHttpReuseAddr(_sock_) \
	{ \
		int	onoff = 1 ; \
		setsockopt( _sock_ , SOL_SOCKET , SO_REUSEADDR , (void *) & onoff , sizeof(int) ); \
	}

#if ( defined __linux ) || ( defined __unix )
#define SetHttpNonblock(_sock_) \
	{ \
		int	opts; \
		opts = fcntl( _sock_ , F_GETFL ); \
		opts |= O_NONBLOCK ; \
		fcntl( _sock_ , F_SETFL , opts ); \
	}
#define SetHttpBlock(_sock_) \
	{ \
		int	opts; \
		opts = fcntl( _sock_ , F_GETFL ); \
		opts &= ~O_NONBLOCK ; \
		fcntl( _sock_ , F_SETFL , opts ); \
	}
#elif ( defined _WIN32 )
#define SetHttpNonblock(_sock_) \
	{ \
		u_long	mode = 1 ; \
		ioctlsocket( _sock_ , FIONBIO , & mode ); \
	}
#define SetHttpBlock(_sock_) \
	{ \
		u_long	mode = 0 ; \
		ioctlsocket( _sock_ , FIONBIO , & mode ); \
	}
#endif

#define SetHttpNodelay(_sock_,_onoff_) \
	{ \
		int	onoff = _onoff_ ; \
		setsockopt( _sock_ , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) ); \
	}

#define SetHttpNoLinger(_sock_,_linger_) \
	{ \
		struct linger   lg; \
		if( _linger_ >= 0 ) \
		{ \
			lg.l_onoff = 1 ; \
			lg.l_linger = _linger_ ; \
		} \
		else \
		{ \
			lg.l_onoff = 0 ; \
			lg.l_linger = 0 ; \
		} \
		setsockopt( _sock_ , SOL_SOCKET , SO_LINGER , (void*) & lg , sizeof(struct linger) ); \
	}

#define SetHttpCloseExec(_sock_) \
	{ \
		int	val ; \
		val = fcntl( _sock_ , F_GETFD ) ; \
		val |= FD_CLOEXEC ; \
		fcntl( _sock_ , F_SETFD , val ); \
	}

_WINDLL_FUNC char *TokenHttpHeaderValue( char *str , char **pp_token , int *p_token_len );

#ifdef __cplusplus
}
#endif

#endif

