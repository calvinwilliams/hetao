STRUCT	hetao_conf
{
	INT	4	worker_processes
	INT	4	cpu_affinity
	INT	4	accept_mutex
	
	STRING	256	error_log
	STRING	6	log_level
	
	STRUCT	limits
	{
		INT	4	max_http_session_count
	}
	
	STRUCT	listen
	{
		STRING	15	ip
		STRING	256	port
	}
	
	STRUCT	ssl
	{
		STRING	256	certificate_file
		STRING	256	certificate_key_file
	}
	
	STRUCT	server
	{
		STRING	256	domain
		STRING	1024	wwwroot
		STRING	1024	index
		STRING	256	access_log
		STRUCT	forward
		{
			STRING	16	forward_type
			STRING	1	forward_rule
			STRUCT	forward_servers
			{
				STRUCT	forward_server	ARRAY	1000
				{
					STRING	15	ip
					INT	4	port
				}
			}
		}
	}
	STRUCT	servers
	{
		STRUCT	server	ARRAY	64
		{
			STRING	256	domain
			STRING	1024	wwwroot
			STRING	1024	index
			STRING	256	access_log
			STRUCT	forward
			{
				STRING	16	forward_type
				STRING	1	forward_rule
				STRUCT	forward_servers
				{
					STRUCT	forward_server	ARRAY	1000
					{
						STRING	15	ip
						INT	4	port
					}
				}
			}
		}
	}
	
	STRUCT	tcp_options
	{
		INT	4	nodelay
		INT	4	nolinger
	}
	
	STRUCT	http_options
	{
		INT	4	compress_on
		INT	4	timeout
		INT	4	forward_disable
	}
	
	STRUCT	error_pages
	{
		STRING	1024	error_page_400
		STRING	1024	error_page_401
		STRING	1024	error_page_403
		STRING	1024	error_page_404
		STRING	1024	error_page_408
		STRING	1024	error_page_500
		STRING	1024	error_page_503
		STRING	1024	error_page_505
	}
	
	STRUCT	mime_types
	{
		STRUCT	mime_type	ARRAY	256
		{
			STRING	50	type
			STRING	100	mime
		}
	}
}

