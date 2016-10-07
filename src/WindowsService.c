#include "hetao_in.h"

static char WINDOWS_SERVICE_NAME[] = "Hetao Service" ;
static char WINDOWS_SERVICE_DESC[] = "High Performance Web Server" ;

SERVICE_STATUS		g_stServiceStatus ;
SERVICE_STATUS_HANDLE	g_hServiceStatusHandle ;

/* 安装WINDOWS服务 */
int InstallService( char *pszConfigPathfilename )
{
	char			szPathFileName[ MAX_PATH + 1 ];
	char			szPathName[ MAX_PATH + 1 ] ;
	char			*p = NULL ;
	char			szStartCommand[ MAX_PATH + 1 ];
	
	SC_HANDLE		schSCManager;
	SC_HANDLE		schService;
	SERVICE_DESCRIPTION	stServiceDescription ;
	
	memset( szPathFileName , 0x00 , sizeof( szPathFileName ) );
	GetModuleFileName( NULL, szPathFileName , sizeof(szPathFileName)-1 );
	
	strcpy( szPathName , szPathFileName );
	p = strrchr( szPathName , '\\' ) ;
	if( p )
	{
		(*p) = '\0' ;
	}
	else
	{
		p = strrchr( szPathName , '/' ) ;
		if( p )
		{
			(*p) = '\0' ;
		}
	}
	p = szPathName ;
	
	memset( szStartCommand , 0x00 , sizeof(szStartCommand) );
	SNPRINTF( szStartCommand , sizeof(szStartCommand)-1 , "\"%s\" \"%s\\%s\" --service" , szPathFileName , szPathName , pszConfigPathfilename );
	
	schSCManager = OpenSCManager( NULL , NULL , SC_MANAGER_CREATE_SERVICE ) ;
	if( schSCManager == NULL )
		return -1;
	
	schService = CreateService( schSCManager ,
				WINDOWS_SERVICE_NAME ,
				WINDOWS_SERVICE_NAME ,
				SERVICE_ALL_ACCESS ,
				SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS ,
				SERVICE_AUTO_START ,
				SERVICE_ERROR_NORMAL ,
				szStartCommand ,
				NULL ,
				NULL ,
				NULL ,
				NULL ,
				NULL );
	if( schService == NULL )
	{
		CloseServiceHandle( schSCManager );
		return -2;
	}
	
	stServiceDescription.lpDescription = (LPTSTR)WINDOWS_SERVICE_DESC ;
	if( ! ChangeServiceConfig2( schService , SERVICE_CONFIG_DESCRIPTION , & stServiceDescription ) )
	{
		CloseServiceHandle( schService );
		CloseServiceHandle( schSCManager );
		return -3;
	}
	
	CloseServiceHandle( schService );
	CloseServiceHandle( schSCManager );
	
	return 0;
}

/* 卸载WINDOWS服务 */
int UninstallService()
{
	SC_HANDLE	schSCManager;
	SC_HANDLE	schService;
	
	schSCManager = OpenSCManager( NULL , NULL , SC_MANAGER_CREATE_SERVICE ) ;
	if( schSCManager == NULL )
		return -1;
	
	schService = OpenService( schSCManager , (LPTSTR)WINDOWS_SERVICE_NAME , SERVICE_ALL_ACCESS ) ;
	if( schService == NULL )
	{
		CloseServiceHandle( schSCManager );
		return -2;
	}
	
	if( DeleteService( schService ) == FALSE )
	{
		CloseServiceHandle( schSCManager );
		return -3;
	}
	
	CloseServiceHandle( schService );
	CloseServiceHandle( schSCManager );
	
	return 0;
}

/* 启动WINDOWS服务 */
int RunService()
{
	SERVICE_TABLE_ENTRY ste[] =
	{
		{ WINDOWS_SERVICE_NAME , ServiceMainProc },
		{ NULL , NULL }
	} ;
	
	if( ! StartServiceCtrlDispatcher( ste ) )
		return -1;
	else
		return 0;
}

/* WINDOWS服务管理函数 */
void WINAPI ServiceCtrlHandler( DWORD dwControl )
{
	switch ( dwControl )
	{
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			g_stServiceStatus.dwCurrentState = SERVICE_STOP_PENDING ;
			SetServiceStatus( g_hServiceStatusHandle , & g_stServiceStatus ) ;
			
			g_stServiceStatus.dwCurrentState = SERVICE_STOPPED ;
			SetServiceStatus( g_hServiceStatusHandle , & g_stServiceStatus) ;
			
			break;
			
		case SERVICE_CONTROL_PAUSE:
			g_stServiceStatus.dwCurrentState = SERVICE_PAUSE_PENDING ;
			SetServiceStatus( g_hServiceStatusHandle , & g_stServiceStatus ) ;
			
			g_stServiceStatus.dwCurrentState = SERVICE_PAUSED ;
			SetServiceStatus( g_hServiceStatusHandle , & g_stServiceStatus) ;
			
			break;
		
		case SERVICE_CONTROL_CONTINUE:
			g_stServiceStatus.dwCurrentState = SERVICE_CONTINUE_PENDING ;
			SetServiceStatus( g_hServiceStatusHandle , & g_stServiceStatus ) ;
			
			g_stServiceStatus.dwCurrentState = SERVICE_RUNNING ;
			SetServiceStatus( g_hServiceStatusHandle , & g_stServiceStatus) ;
			
			break;
			
		case SERVICE_CONTROL_INTERROGATE:
			g_stServiceStatus.dwCurrentState = SERVICE_RUNNING ;
			SetServiceStatus( g_hServiceStatusHandle , & g_stServiceStatus) ;
			
			break;
			
		default:
			break;
			
	}
	
	return;
}

/* 启动WINDOWS服务 */
void WINAPI ServiceMainProc( DWORD argc , LPTSTR *argv )
{
	g_stServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS ;
	g_stServiceStatus.dwCurrentState = SERVICE_START_PENDING ;
	g_stServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP ;
	g_stServiceStatus.dwWin32ExitCode = 0 ;
	g_stServiceStatus.dwServiceSpecificExitCode = 0 ;
	g_stServiceStatus.dwCheckPoint = 0 ;
	g_stServiceStatus.dwWaitHint = 0 ;
	
	g_hServiceStatusHandle = RegisterServiceCtrlHandler( (LPTSTR)WINDOWS_SERVICE_NAME , ServiceCtrlHandler ) ;
	if( g_hServiceStatusHandle == (SERVICE_STATUS_HANDLE)0 )
		return;
	
	g_stServiceStatus.dwCheckPoint = 0 ;
	g_stServiceStatus.dwWaitHint = 0 ;
	g_stServiceStatus.dwCurrentState = SERVICE_RUNNING ;
	
	SetServiceStatus( g_hServiceStatusHandle , & g_stServiceStatus );
	
	MonitorProcess( g_p_env );
	
	return;
}
