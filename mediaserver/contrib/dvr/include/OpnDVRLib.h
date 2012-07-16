#include "windows.h"
#include "mmsystem.h"

#ifndef _OPNDVRLIB_H_
#define _OPNDVRLIB_H_

#define DVRVERSION_MAJOR    1
#define DVRVERSION_MINOR    0
#define DVRVERSION_RELEASE  4	
#define DVRVERSION_BUILD    0	

/*
------------------------------------------------------------------------------------

   CALLBACK Windows' wParam and lParam values.

------------------------------------------------------------------------------------
*/

typedef HANDLE DVRHANDLE;
typedef DVRHANDLE* LPDVRHANDLE;
typedef HANDLE DVRSEARCHHANDLE;
typedef DVRSEARCHHANDLE * LPDVRSEARCHHANDLE;

typedef enum {
  EVENT_EX_ALARM = 0xF0,
  EVENT_NOSIGNAL,     // video loss event, 0: No signal, 1: Signal	  
  EVENT_SENSOR,              // Sensor trigger status, 0: Normal, 1: Sensor triggered 
  EVENT_CONTROL,             // Relay out status, 0: OFF, 1: ON 
  EVENT_BEEP,                // Beep status based on Server setting, 0: Normal, 1: Beeping 
  EVENT_MOTION,              // Motion detection status: 0: Normal, 1: Motion detected.
  EVENT_NOTIFY               // Connection status between Server and Agent  				
}
DVREventKind;

#define NOTIFY_SITECONNECT     0x01   // Socket communication at server started.
#define NOTIFY_SITEDISCONNECT  0x02   // Socket communication at server stopped.
#define NOTIFY_REQUESTPTZUNLOCK  0x03   // PTZ Unlock request.

/*
------------------------------------------------------------------------------------
On DVR event, Callback Windows handler sends notification (EVENT_XXXX) to Agent with information in wParam.
EVENT_NOTIFY: Event occurs only on connection and disconnection
Other than EVENT_NOTIFY, lParam and wParam values (each bit) stands for Camera, Sensor, Relay out status

EVENT_CONTROL:	Bit			0	1	2	....	15
				Relay out	16	15	14	....	1

Other:			Bit			0	1	2	....	16	....	32
				Camera		1	2	3	....	16	....	32
				Sensor		1	2	3	....	16

If 0 < wParam < 4, 8, 16, 32 (Camera depends on the DVR system), then this event contains
Live Video frame associated with the camera (wParam).

If wParam i > 0xF1, then it recognize it as DVR event: Video loss, Sensor, Relay Out, Motion...
-------------------------------------------------------------------------------------

  [-] SAMPLE CODE
    
	  #define WM_LIVEMSG WM_USER + 1

       .
	   .

    DVRHANDLE    g_ConnectHandle;
	HWND        g_hWnd;
    DVRSiteInfo  g_SiteInfo;

    LRESULT WINAPI WndProc( HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam )
	{
	   if (Msg==WM_LIVEMSG) 
	   {
	       if (wParam > 0 && wParam <= g_SiteInfo.nCameraCount)
		   {			  	
		      char * Buffer;
			  RECT R;

			  // Save Jpeg Image Routine	
			  // Buffer = new Buffer[lParam];
		      // DVRGetLiveImage( g_ConnectHandle, wParam, Buffer, lParam );
			  
              // Draw Live Image
              GetClientRect( hwnd, &R );
			  DVRDrawLiveImage( g_ConnectHandle, wParam, GetDC(hwnd),  R );
		   }
		   else {
		      int i;
		      switch (wParam)
			  {
			     case EVENT_NOSIGNAL: 
				   for (i=0; i<g_SiteInfo.nCameraCount; i++)
				   {
				      if ((lParam & (1<<i)) == 0)
					  {
					     printf("Camera %s No Signal!\n",i+1);
					  }
				   }
				   // No Signal 
				   break;
                 case EVENT_SENSOR:
				   // Sensor
				   break;
                 case EVENT_CONTRL:
				   // Control
				   break;
                 case EVENT_BEEP:
				   // Beep
				   break;
                 case EVENT_MOTION:
				   // Motion
				   break;
			  }
		   }
		   return 1;
	   }
	   else return DefWindowProc( hwnd, Msg, wParam, lParam );
	}


	StartLive()    
	{
        ...  
        g_ConnectHandle = DVROpenConnection();
		if (DVRConnectSite( g_ConnectHandle, 'localhost', 2000, 'Administrator', '' )==DVRErrOK)
		{
		    g_SiteInfo.cbSize = sizeof( DVRSiteInfo );
		    DVRGetSiteInfo( g_ConnectHandle, &g_SiteInfo );
            DVRStartLive( g_ConnectHandle, g_hWnd, WM_LIVEMSG );
			DVRSetLiveMask( g_ConnectHandle, 0xFFFF );
		}
		else DVRCloseConnect( g_ConnectHandle );

	}

    CloseLive()
	{
		DVRCloseConnection( g_ConnectHandle );
	}

------------------------------------------------------------------------------------
*/

typedef enum {
	DVRErrOK,                   // Routine completed successfully
	DVRErrInvalidHandle,        // Invalid Windows handle value
	DVRErrInvalidOperation,     // Invalid command
	DVRErrInvalidParam,         // Invalid parameter value
	DVRErrNotImplemented,       // Not inplemented yet
	DVRErrUserError,            // Invalid User name 
	DVRErrPassError,            // Invalid Password
	DVRErrOperationFail,        // Operation failed
	DVRErrSocketError,          // TCP/IP Socket connection error
	DVRErrSocketConnectTimeout, // TCP/IP Socket connection Timed out
	DVRErrSocketReadTimeout,    // TCP/IP Socket reading operation timed out
	DVRErrExistsConnection,     // Another connection is already establised.
	DVRErrNoExistConnection,    // No connection yet, please make a connection first.
	DVRErrSearchNoFile,		  		// Search File but, no exist.
	DVRErrSetupOperationFail,   // Invalid Setup Command
	DVRErrSearchSeekTime,
	DVRErrAlreadyInUse,
	DVRErrAlreadyOpen,
	DVRErrNotOpened,
	DVRErrPermissionDenind,
	DVRErrConnectionRefused,
	DVRErrLocalSearch_Fail,
	DVRErrLocalSearch_NoSearch,
	DVRErrLocalSearch_NoLogin,
	DVRErrLocalSearch_OnSetup,
	DVRErrLocalSearch_OnMessageWnd,
	DVRErrLocalSearch_OnSearch,
	DVRErrLocalSearch_OutSearch,

	DVRErrDatabaseOpenFail,
	DVRErrDatabaseNotExist,

    DVRErrCannotStart,
    DVRErrNotStarted,
    DVRErrModuleNotLoaded,
	DVRErrWarning,
	DVRErrTimedOut,

	DVRErrNotSupport,
	DVRErrMaxConnectionExceed,
	DVRErrUnsupportFirmware
}
DVRRESULT; 

typedef enum {
	DVR_LocalSearchStaus_Pause        = 1, // GET/SET
	DVR_LocalSearchStaus_Play         = 2, // GET/SET
	DVR_LocalSearchStaus_FastForward  = 3, // GET/SET
	DVR_LocalSearchStaus_FastBackward = 4, // GET/SET
	DVR_LocalSearchStaus_StepForward  = 5  //     SET
} DVRLocalSearchStatus;

typedef enum {
	DVRVideoDataType_Original		  = 0,
	DVRVideoDataType_Decompressed	  = 1
} DVRVideoDataType;

// Initialize Open API DLL

BOOL DVRInitLibrary(BOOL bIsWindowApp=TRUE);


// Close Open API DLL 

void DVRCloseLibrary(void);
void DVRFreeLibrary(void);

DWORD WINAPI DVRGetLibraryVersion(void);

/*
###############################################################################
  
  [-] COMMENT:
    
	Open Handle to connect DVR(s). If you want to connect multiple DVRs, 
	and then you have to call this function as many you want.
	You must close the opened handle using DVRCloseConnection()
  
  [-] PARAMETERSS:
    
	None

  [-] RETURN VALUE:

    Handle Value
 
###############################################################################
*/

DVRHANDLE WINAPI DVROpenConnection(void);

#include <DeviceType.h>

DVRHANDLE WINAPI DVROpenConnection2(DWORD dwDeviceType);

/*
###############################################################################
  
  [-] COMMENT:
    
	Return all opened Handles by DVROpenConnection(), deallocate memory, and stop all jobs (live vide..)
	
  [-] PARAMETERS:
    
	Handle 
	  - Retruned Handle from DVROpenConnection()

  [-] RETURN VALUES:

    DVRErrOK
	  - Success
    DVRInvalidHandle
	  - Wrong handle value
 
###############################################################################
*/

DVRRESULT WINAPI DVRCloseConnection( 
     DVRHANDLE Handle 
);

/*
###############################################################################

Auto detect device type such as Device Model,Connect Port.
This function use a web port.

###############################################################################
*/

typedef struct {
	DWORD	cbSize;    
    DWORD	dwDeviceType;
    CHAR	szModelName[40];
    WORD	wPort;
    WORD	wDummy;
    WORD	wVideoCount;
    WORD	wAudioCount;
    WORD	wSensorCount;
    WORD	wRelayCount;
}
DVRDeviceInfo, *LPDVRDeviceInfo;


DVRRESULT WINAPI DVRDeviceAutoDetect(
	 LPCSTR szSiteIP,
	 USHORT usPort,
	 LPDVRDeviceInfo lpDeviceInfo);

/*
###############################################################################

Check device type was supported from current API.

###############################################################################
*/


BOOL WINAPI DVRCheckDevice(
	 DWORD dwDeviceType
);

/*
###############################################################################

Check device capability by device type or opend handle.

###############################################################################
*/

  // In use DVRCheckDeviceCaps API
#define DEVICECAPS_LIVEVIDEO		  1000
#define DEVICECAPS_LIVEAUDIO          1100
#define DEVICECAPS_PTZCONTROL         1200
#define DEVICECAPS_SENSORFIRE         1300
#define DEVICECAPS_RELAYCONTROL       1400
#define DEVICECAPS_TALK               1500
#define DEVICECAPS_REMOTESEARCH       1600
#define DEVICECAPS_SPEEDCONTROLTYPE   1601
#define   SPEEDCONTROLTYPE_1          1
#define   SPEEDCONTROLTYPE_2          2
#define DEVICECAPS_RECORDSTATUS_DAY   1602
#define DEVICECAPS_RECORDSTATUS_MONTH 1603
#define DEVICECAPS_SPEEDMIN		      1604
#define DEVICECAPS_SPEEDMAX		      1605
#define DEVICECAPS_FORCEINTENSIVE     1700
#define DEVICECAPS_INSTANTRECORDING   1800
#define DEVICECAPS_FORCEFULLSCREEN    1900
#define DEVICECAPS_FORCEDIVISION      1901    
#define DEVICECAPS_ALARMMONITOR       2000
#define DEVICECAPS_HEALTHCHECK        2100
#define DEVICECAPS_FRAMESETUP         3000
#define DEVICECAPS_LOGVIEW            3100
#define DEVICECAPS_CMD_DEVICESETUP    5000
#define DEVICECAPS_TVOUT1             10001
#define DEVICECAPS_TVOUT2             10002
#define DEVICECAPS_LOCALSEARCH        10100
#define DEVICECAPS_TRANSACTIONSEARCH  10101
#define DEVICECAPS_CLIPSEARCH         10102
#define DEVICECAPS_TOWAYAUDIO         10200
#define DEVICECAPS_AUTOLOGIN          10300
#define DEVICECAPS_AUDIOSEARCH		  10400

INT WINAPI DVRCheckDeviceCaps(
	 DWORD dwDeviceType,
	 INT nCapability);

INT WINAPI DVRCheckSiteCaps(
	 DVRHANDLE Handle,
	 INT nCapability);


/*
###############################################################################

DVRSetInstallCode

###############################################################################
*/

void WINAPI DVRSetInstallCode(
	 DVRHANDLE Handle,
	 DWORD dwInstallCode,
	 DWORD dwSubInstallCode
);

/*
###############################################################################
  
  [-] COMMENT:
    
	It tries to connect to DVR via TCP/IP. It can receive all DVR information including Camera and Sensor settings.
  
  [-] PARAMETERS:
    
	Handle 
	  - Returned from DVROpenConnection()
	SiteIP
	  - DVR IP address. It connects locally without password confirmation if this value is NULL.
	SitePort
	  - DVR port number
	UserID
	  - DVR User ID. It chooses "Administrator" if this value is NULL
	Password
	  - Password. It will be ignored if SiteIP value is NULL
	AReadTimeOut
	  - Remote ReadTimeOut. This time is millisecond. 

  [-] RETURN VALUES:

    DVRErrOK
	  - Completed Successfully
    DVRInvalidHandle
	  - Invalid Handle
	DVRUserError
	  - Unregistered User name
	DVRPassError
	  - Wrong password
	DVRSocketConnectTimeout
	  - TCP/IP Socket connection timed out
	DVRSocketReadTimeout
	  - TCP/IP Socket reading timed out
	DVRSocketError
	  - Socket connection fail
    DVROperationFail
	  - Unknown Error occured
 
###############################################################################
*/

DVRRESULT WINAPI DVRConnectSite(
     DVRHANDLE Handle,
     LPCSTR SiteIP,
     INT SitePort,
     LPCSTR UserID,
     LPCSTR Password,
	 INT AReadTimeOut
);


/*
###############################################################################
  
  [-] COMMENT:
    
		    
  [-] PARAMETERS:
    

  [-] RETURN VALUES:

 
###############################################################################
*/

DVRRESULT WINAPI DVRDisconnectSite( 
     DVRHANDLE Handle 
);

/*
###############################################################################
  
  [-] COMMENT:
    
		    
  [-] PARAMETERS:
    

  [-] RETURN VALUES:

 
###############################################################################
*/


BOOL WINAPI DVRIsConnected( 
     DVRHANDLE Handle 
);

/*
###############################################################################
  
  [-] COMMENT:
    
		    
  [-] PARAMETERS:
    

  [-] RETURN VALUES:

 
###############################################################################
*/

DVRRESULT WINAPI DVRControlDevice( 
     DVRHANDLE Handle,
     INT nControlCmd,
     LPVOID pControlData);

/*
###############################################################################
  
  [-] COMMENT:
    
		    
  [-] PARAMETERS:
    

  [-] RETURN VALUES:

 
###############################################################################
*/

DVRRESULT WINAPI DVRQueryDevice( 
     DVRHANDLE Handle,
     INT nQueryCmd,
	 INT nArg,
     LPVOID pQueryData);


/*
###############################################################################
  
  [-] COMMENT:
    
		    
  [-] PARAMETERS:
    

  [-] RETURN VALUES:

 
###############################################################################
*/
DVRRESULT WINAPI DVRSetRelay( 
	 DVRHANDLE Handle, 
	 INT RelayNo, BOOL bOnOff 
);

/*
###############################################################################
  
  [-] COMMENT:
    
		    
  [-] PARAMETERS:
    

  [-] RETURN VALUES:

 
###############################################################################
*/
DVRRESULT WINAPI DVRSensorFire( 
	 DVRHANDLE Handle, 
	 INT SensorNo, BOOL bOnOff 
);

/*
###############################################################################
  
  [-] COMMENT:

	Set Callback Flag

  [-] PARAMETERS:
    
	Handle 
	  - Handle value from DVROpenConnection()

    dwCallbackFlag

     - CALLBACKFLAG_ASYNC  : Select Async Callback (Default Sync Callback)

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrNoExistConnection
 
###############################################################################
*/

#define CALLBACKFLAG_ASYNC	1

DVRRESULT WINAPI DVRSetCallbackFlag( 
     DVRHANDLE Handle, 
     DWORD dwCallbackFlag
);


/*
###############################################################################
  
  [-] COMMENT:

	It initiates live monitoring of DVR's Sensor, Relay Out and camera status.
	It connects to DVR Live Port and independant Thread starts polling all of DVR events.
	You must call DVRStopLive() to clear this monitoring.

  [-] PARAMETERS:
    
	Handle 
	  - Handle value from DVROpenConnection()
	WindowHandle
	  - Windows handle that receives Event Message from DVR
	    If WindowHandle is zero, CallBack Function will be run. 
	CallBack
	  - CallBack function when Event occurs
	CallbackInstance
	  - User-instance data passed to the callback mechanism. 
		This parameter is not used with the window callback mechanism.


  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrNoExistConnection
 
###############################################################################
*/

DVRRESULT WINAPI DVRStartLive( 
     DVRHANDLE Handle, 
     HWND WindowHandle, 
     DWORD CallBack,
	 DWORD CallbackInstance
);

/*
###############################################################################
  
  [-] COMMENT:

	It initiates live monitoring of DVR's Sensor, Relay Out and camera status.
	It connects to DVR Live Port and independant Thread starts polling all of DVR events.
	You must call DVRStopLive() to clear this monitoring.

	This function is designed for extended version of DVRStartLive.
	The last parameter of this function make user to be able to inform receiving live data type.

  [-] PARAMETERS:
    
	Handle 
	  - Handle value from DVROpenConnection()
	CallBack
	  - CallBack function when Event occurs
	CallbackInstance
	  - User-instance data passed to the callback mechanism. 
		This parameter is not used with the window callback mechanism.
	VideoDataType
	  - User intended receiving live data type.

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrNoExistConnection
 
###############################################################################
*/

DVRRESULT WINAPI DVRStartLiveEx( 
     DVRHANDLE Handle, 	 
     DWORD CallBack,
	 DWORD CallbackInstance,
	 DVRVideoDataType VideoDataType
);

/*
###############################################################################
  
  [-] COMMENT:
   
	It stops DVR monitoring and an independant Live Polling Thread
	  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrNoExistConnection
 
###############################################################################
*/

DVRRESULT WINAPI DVRStopLive( 
     DVRHANDLE Handle 
);

/*
###############################################################################
  
  [-] COMMENT:
	It sets a camera which you want to receive live vide frame from DVR.
	You must call DVRStartLive() function before this function.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()
	dwLiveMask
	  - Camera setting in Bit mask for live video transmission. LSB --> Camera 1, MSB --> Camera 32
		Set specific bit (camera) on to receive live video.

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrNoExistConnection
 
###############################################################################
*/

DVRRESULT WINAPI DVRSetLiveMask( 
     DVRHANDLE Handle, 
     DWORD dwLiveMask
);

/*
###############################################################################
  
  [-] COMMENT:

	It defines live video frame type (JPG or Bitmap) when you call DVRGetLiveImage() function.
	If 0 < wParam < 4, 8, 16, 32 (Camera depends on the DVR system), then it means live video transmission.
	lParam is the size of its image frame size and you have to reserve this amount of memory buffer.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()
	ImgFmt
      - DVRImgFmtJPEG   : Receive JPEG format video frame
      - DVRImgFmtBITMAP : Receive Bitmap format video frame

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRInvalidParam
	DVRErrNoExistConnection

###############################################################################
*/

typedef enum {
	  DVRImgFmtJPEG,
	  DVRImgFmtBITMAP
}
DVRIMAGEFORMAT, *LPDVRIMAGEFORMAT;


DVRRESULT WINAPI DVRSetImageFormat(
	 DVRHANDLE Handle,
	 DVRIMAGEFORMAT ImgFmt
);

DVRRESULT WINAPI DVRGetImageFormat(
     DVRHANDLE Handle,
	 LPDVRIMAGEFORMAT lpImgFmt
);

/*
###############################################################################
  
  [-] COMMENT:

	It copies the specified camera's last frame into specified memory buffer.
	Its image format in JPEG.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()
	CameraNo
	  - Camera number to receive live video (1..Max Camera)
	Buffer
	  - Reserved memory buffer. You can get the image buffer size if you set it NULL.

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRInvalidParam
	DVRErrNoExistConnection
 

###############################################################################
*/

DVRRESULT WINAPI DVRGetLiveImage( 
     DVRHANDLE Handle, 
     INT CameraNo, 
     LPSTR Buffer, 
     LPDWORD Size
); 

/*
###############################################################################
  
  [-] COMMENT:
    
	It displays received frame onto reserved DC area.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()
	CameraNo
	  - Camera number to be displayed (1.. Max Camera)
	DestDC
	  - Destination of Device Context Handle that image to be displayed.
	DestRect
	  - Target region (rectangle) of DC that image to be displayed.

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrNoExistConnection
 
###############################################################################
*/

DVRRESULT WINAPI DVRDrawLiveImage( 
     DVRHANDLE Handle, 
	 INT CameraNo, 
	 HDC DestDC, 
	 LPRECT lpDestRect 
);

/*DVRRESULT WINAPI DVRDrawLiveImage_VB( 
     HANDLE Handle, 
	 INT CameraNo, 
	 HDC DestDC, 
	 LPRECT DestRect 
);*/


/*
###############################################################################
  
  [-] COMMENT:
    It requests DVR's detail information.
	A connection by DVRConnectSite() function must be established before this.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()
	lpSiteInfo
	  - Structure of LPDVRSiteInfo. It must not be NULL.
		This structure's variable "cbSize" must contain this structure's size calculated by sizeof() function.

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRInvalidParam
	DVRErrNoExistConnection

###############################################################################
*/

#pragma pack( push, enter_include1 )
#pragma pack(8)

//-----------------------------------------------------------------------------
// Data structure to call DVRGetCameraInfo() function
// You must pass this structure with its size in cdSize variable

typedef struct {
    DWORD cbSize;                // size of this structure
    BOOL  bUseCamera;            // Camera usage status TRUE / FALSE
    BOOL  bUsePanTilt;           // Pan/Tilt usage status TRUE / FALSE
    char  lpszPositionName[15];  // Camera name 15 characters
    DWORD dwSensorBit;           // Related sensor with this camera
}
DVRCameraInfo,*LPDVRCameraInfo;

//-----------------------------------------------------------------------------
// Data structure to call DVRGetSiteInfo() function
// You must pass this structure with its size in cdSize variable

typedef struct {
    DWORD cbSize;                // Size of this data structure
    char  lpszHost[51];          // DVR IP address
    INT   nPort;                 // DVR Port
    INT   nCameraCount;          // Total number of camera
    BOOL  bIsPal;                // Camera Type: NTSC(FALSE) / PAL(TRUE)
    WORD  wMaxFrame;             // Maximum Frame
    DWORD dwSiteVersion;         // DVR S/W Version
    TIME_ZONE_INFORMATION TimeZone;
}
DVRSiteInfo, *LPDVRSiteInfo;

typedef struct {
	DWORD	cbSize;
	int		nCameraCount;
	int		nSensorCount;
	int		nRelayCount;
	int		nAudioCount;
	CHAR    Reserved[256];
}
DVRHardwareInfo, *LPDVRHardwareInfo;

typedef struct {
	DWORD	cbSize;
	int		nCameraNo;
	int		nWidth;
	int		nHeight;
	SYSTEMTIME FrameTime;
	void*	lpEncFrameData;
	DWORD	dwEncFrameSize;
	DWORD	dwDecFourCC;
	void*	lpDecFrameData;
	DWORD	dwDecFrameSize;
}
DVRCaptureData, *LPDVRCaptureData;

#pragma pack( pop, enter_include1 )


DVRRESULT WINAPI DVRGetSiteInfo( 
     DVRHANDLE Handle,
     LPDVRSiteInfo lpSiteInfo 
);

DVRRESULT WINAPI DVRGetHardwareInfo( 
     DVRHANDLE Handle,
     LPDVRHardwareInfo lpHardwareInfo 
);

/*
###############################################################################
  
  [-] COMMENT:
    
	It requests all Camera information.
	A connection by DVRConnectSite() function must be established before this.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()
	CameraNo
	  - Camera number to be requested(1..Max Camera)
	lpCameraInfo
	  - Structure of LPDVRCameraInfo. It must not be NULL.
		This structure's variable "cbSize" must contain this structure's size calculated by sizeof() function.

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRInvalidParam
	DVRErrNoExistConnection

###############################################################################
*/

DVRRESULT WINAPI DVRGetCameraInfo( 
     DVRHANDLE Handle,
     INT CameraNo,
     LPDVRCameraInfo lpCameraInfo
);


/*
###############################################################################
  
  [-] COMMENT:
    
	It requests and receive the status of Camera, Sensor, Relay out, motion, and Beep of connected DVR.
	A connection by DVRConnectSite() function must be established before this.
  
  [-] PARAMETERS:
    
	Handle 
	  - A handle from DVROpenConnection()
	CameraNo
	  - Camera number to be requested (1 ~ Max Camera:4,8,16,32)
	lpdwStatus
	  - It contains connected DVR's No Signal, Sensor, Control, Beep, Motion, Hidden Camera,
	    Permission status in bit mask format.
		You can get detail each bit's value by "AND" operation with constant value CAM_STAT_XXX .

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRInvalidParam
	DVRErrNoExistConnection

###############################################################################
*/

#define  CAM_STAT_NOSIGNAL    1                  // No-Signal
//#define  CAM_STAT_SENSOR      2         
//#define  CAM_STAT_CONTROL     4
//#define  CAM_STAT_BEEP        8
#define  CAM_STAT_MOTION      16
#define  CAM_STAT_HIDE        32
//#define  CAM_STAT_PERMISSION  64         

DVRRESULT WINAPI DVRGetCameraStatus( 
     DVRHANDLE Handle,
     INT CameraNo,
     LPDWORD lpdwStatus
);

DVRRESULT WINAPI DVRGetRelayStatus( 
     DVRHANDLE Handle,
     LPDWORD lpdwStatus
);

DVRRESULT WINAPI DVRGetSensorStatus( 
     DVRHANDLE Handle,
     LPDWORD lpdwStatus
);


/*
###############################################################################
  
  [-] COMMENT:
    
	It requests and receives connected DVR status. 
	A connection by DVRConnectSite() function must be established before this.
  
  [-] PARAMETERS:
    
	Handle 
	  - A handle from DVROpenConnection() function
	lpdwStatus
	  - Each bit stands for a specified camera's connection and Live status.
		You can calculate each bit's value by "AND" operation with constant value SITE_STAT_XXX.

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrNoExistConnection

###############################################################################
*/

#define SITE_STAT_CONNECTED   1    // It is connected to DVR by DVRConnectSite() function
#define SITE_STAT_LIVE        2    // It is receiving DVR's live status by DVRStartLive() function

DVRRESULT WINAPI DVRGetSiteStatus( 
     DVRHANDLE Handle,
     LPDWORD lpdwStatus
);


/*
###############################################################################
  
  [-] COMMENT:
    
	It requests and receives connected DVR status. 
	A connection by DVRConnectSite() function must be established before this.
  
  [-] PARAMETERS:
    
	Handle 
	  - A handle from DVROpenConnection() function
	lpSiteInfo
	  - Pointer of DVRSiteInfoStructure

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrNoExistConnection

###############################################################################
*/

typedef BYTE	DVRVIDEOQUALITY;

#define VIDEOQUALITY_LOW		DVRVIDEOQUALITY(0)
#define VIDEOQUALITY_MIDDLE     DVRVIDEOQUALITY(1)
#define VIDEOQUALITY_FINE       DVRVIDEOQUALITY(2)
#define VIDEOQUALITY_SUPERFINE  DVRVIDEOQUALITY(3)
#define VIDEOQUALITY_ULTRAFINE  DVRVIDEOQUALITY(4)

typedef BYTE	DVRVIDEORESOLUTION;

#define VIDEORESOLUTION_QUAD	DVRVIDEORESOLUTION(0)
#define VIDEORESOLUTION_HALF	DVRVIDEORESOLUTION(1)
#define VIDEORESOLUTION_FULL	DVRVIDEORESOLUTION(2)

typedef struct { 
    WORD				nPreAlarmTime;
    WORD				nMotionPostAlarmTime;
    WORD				nSensorPostAlarmTime;
    BYTE				nFrameRate;
    DVRVIDEORESOLUTION	nResolution;
    DVRVIDEOQUALITY		nQuality;
}
DVRCameraInfoStructure, *LPDVRCameraInfoStructure;

typedef struct {
    DWORD					cbSize;
    WORD					wSiteID;
    DVRCameraInfoStructure	CameraInfo[64];
}
DVRSiteInfoStructure, *LPDVRSiteInfoStructure;


DVRRESULT WINAPI DVRGetSiteInfoStructure( 
	DVRHANDLE	Handle,
	LPDVRSiteInfoStructure	lpSiteInfo 
);

DVRRESULT WINAPI DVRSetInstantRecordStatus(
	DVRHANDLE	Handle,
	INT			CameraNo,
	BOOL		bOnOff
);

/*
###############################################################################
  
  [-] COMMENT:
    
	It requests and receives connected DVR status. 
	A connection by DVRConnectSite() function must be established before this.
  
  [-] PARAMETERS:
    
	Handle 
	  - A handle from DVROpenConnection() function
	nCameraNo
	  - Camera No (0-Base)

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrNoExistConnection

###############################################################################
*/


DVRRESULT WINAPI DVRForceFullScreen( 
     DVRHANDLE Handle,
     INT CameraNo         // FullScreen 으로 전환할 카메라 번호 ( 0~Max Camera-1 )
);

#define DIVISION_4A (0xFF + 0)
#define DIVISION_4B (0xFF + 1)
#define DIVISION_4C (0xFF + 2)
#define DIVISION_4D (0xFF + 3)
#define DIVISION_7  (0xFF + 4)
#define DIVISION_9A (0xFF + 5)
#define DIVISION_9B (0xFF + 6)
#define DIVISION_10 (0xFF + 7)
#define DIVISION_13 (0xFF + 8)
#define DIVISION_16 (0xFF + 9)


DVRRESULT WINAPI DVRForceScreenDivision( 
     DVRHANDLE Handle,
     INT nDivision,       // DIVISION_XX Constant
	 INT nBoard           // Board Number, 32 채널 보드에서는 0 = 1~16 채널 , 1 = 17~32 채널
);


DVRRESULT WINAPI DVRGetTVOutType( 
	 DVRHANDLE Handle,
	 INT * nTVOutType 
);

#define TVOUTMODE_OFF		0
#define TVOUTMODE_AUTO		1
#define TVOUTMODE_CLONE		2
#define TVOUTMODE_DIVISION	3

DVRRESULT WINAPI DVRChangeTVOut( 
	 DVRHANDLE Handle,
	 INT nCameraNo,
     INT nPortNo,
     INT nTVOutMode );

/*
###############################################################################
  
  [-] COMMENT:

	특정 카메라를 일시적으로 Intensive Recording 상태가 되도록 한다.
  
  [-] PARAMETERS:
    
	Handle 
	  - A handle from DVROpenConnection() function
	CameraNo
	  - Camera number to be displayed (1.. Max Camera)

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRInvalidParam
	DVRErrOperationFail
	DVRErrNoExistConnection

###############################################################################
*/

DVRRESULT WINAPI DVRForceIntensive( 
	DVRHANDLE Handle,
	INT CameraNo
);

/*
###############################################################################
  
  [-] COMMENT:

	Handle 로 지정한 Open 된 DVR 로 검색을 위해 접속을 시도하고 검색용 핸들을
	SearchHandle 파라메터로 반환한다.
  
  [-] PARAMETERS:
    
	Handle 
	  - A handle from DVROpenConnection() function
	SearchHandle
	  - 이 함수 호출이 성공했을 때 검색을 위한 Handle 값을 돌려 받는다.
	    이 값은 NULL 이어서는 안된다.

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRInvalidParam
	DVRErrSocketError

###############################################################################
*/
  
DVRRESULT WINAPI DVRSearchOpen( 
	DVRHANDLE Handle,
	LPDVRSEARCHHANDLE lpSearchHandle
);

/*
###############################################################################
  
  [-] COMMENT:

	DVRSearchOpen 을 통해 얻어진 검색용 핸들을 Close 한다.
  
  [-] PARAMETERS:
    
	SearchHandle
	  - Handle from DVRSearchOpen()

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle

###############################################################################
*/

DVRRESULT WINAPI DVRSearchClose( 
	DVRSEARCHHANDLE SearchHandle
);

/*
###############################################################################
  
  [-] COMMENT:

	검색한 카메라의 비트 마스크를 지정한다. 
  
  [-] PARAMETERS:
    
	SearchHandle
	  - Handle from DVRSearchOpen()
	dwCamera
	  - 카메라 미트 마스트 값이다. 최하의 비트가 카메라 1번을 나타낸다.

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle

###############################################################################
*/

DVRRESULT WINAPI DVRSearchSetCamera( 
	DVRSEARCHHANDLE SearchHandle, 
	DWORD dwCamera
);

typedef INT	DVREVENTTYPE;

#define EVENTTYPE_SENSOR				DVREVENTTYPE(1)
#define EVENTTYPE_SENSOR_PREALARM		DVREVENTTYPE(2)
#define EVENTTYPE_MOTION				DVREVENTTYPE(3)
#define EVENTTYPE_MOTION_PREALARM		DVREVENTTYPE(4)
#define EVENTTYPE_INSTANT				DVREVENTTYPE(5)
#define EVENTTYPE_ALL					DVREVENTTYPE(6)
#define	EVENTTYPE_SENSOR_POSTALARM		DVREVENTTYPE(10)
#define EVENTTYPE_MOTION_POSTALARM		DVREVENTTYPE(11)

typedef struct tagDVRSEARCHRESULT
{
	INT				nCamera;		// 검색된 카메라 번호
    SYSTEMTIME		SearchTime;		// 검색된 날짜, 시간
    INT				nSensorNo;		// 관련된 센서 번호
    DVREVENTTYPE	nRecordType;	// 레코딩 상태
    DWORD			dwImageSize;	// 최종 검색된 비트맵 영상의 크기
	WORD			wWidth;
	WORD			wHeight;
}
DVRSEARCHRESULT, *LPDVRSEARCHRESULT;

#define SEARCHFORMAT_ORIGINAL      0x01    // Orignal stream format
#define SEARCHFORMAT_COMPRESSED    0x02    // Force compress video even if not compressed from orignal stream
#define SEARCHFORMAT_DECOMPRESSED  0x03    // Force decompress video : It can be output to RGB24 or YUV422 format
#define SEARCHFORMAT_FORCE_JPEG    0x80    // Force compress to JPEG
#define SEARCHFORMAT_FORCE_RGB24   0xF1    // Force decompress to RGB24 format
#define SEARCHFORMAT_FORCE_YUV422  0xF2    // Force compress to YUV422 format

typedef struct tagDVRSEARCHOPTION
{
	DWORD			cbSize;
	SYSTEMTIME		SearchTime;
	BOOL			IsDST;
	DWORD			VideoFormat;
}
DVRSEARCHOPTION, *LPDVRSEARCHOPTION;

typedef struct tagDVRSEARCHRESULTEX
{
	DVRSEARCHRESULT	Result;
	BOOL			IsDST;
    BOOL			IsKeyFrame;
    void *			EncFrameData;
    DWORD			EncFrameSize;
    DWORD			DecFourCC;
    void *			DecFrameData;
    DWORD			DecFrameSize;
    BYTE			Reserved[8];
}
DVRSEARCHRESULTEX, *LPDVRSEARCHRESULTEX;

typedef struct tagDVRSEARCHAUDIORESULT
{
	INT				Channel;
	DVRRESULT		AudioResult;
	DWORD			AudioSize;
	SYSTEMTIME		StartTime;
	SYSTEMTIME		EndTime;
	WAVEFORMATEX	Wfx;
}
DVRSEARCHAUDIORESULT, *LPDVRSEARCHAUDIORESULT;

typedef struct tagWAVEFORMAT
{
	char riffID[4];
	DWORD riffSIZE;
	char riffFORMAT[4];
	char fmtID[4];
	DWORD FmtSIZE;
	WAVEFORMATEX Wfx;
	char dataID[4];
	DWORD dataSIZE;
	PBYTE data;
}WAVEFORMATSTR, *LPWAVEFORMATSTR;

typedef struct tagDUMYWORD
{
	WORD one;
	WORD two;
	WORD three;
	WORD four;	
}
DUMYWORD, *LPDUMYWORD;

typedef struct tagDVRTRANSACTION
{
	BYTE CameraNum;
	BYTE SensorNum;
	BYTE AlarmType;
	SYSTEMTIME EventTime;
	DWORD DumyOne[4];
	DUMYWORD DumyTwo;
	BYTE DumyThree;
}
DVRTRANSACTION, *LPDVRTRANSACTION;

typedef struct tagTIDSEARCH
{
	char SearchID[128];
	SYSTEMTIME SearhchTime;
}
TIDSEARCH, *LPTIDSEARCH;

typedef struct tagTIDSEARCHREAL
{
	char SearchID[128];
	DWORD SearchTime;
}
TIDSEARCHREAL, *LPTIDSEARCHREAL;


/*
###############################################################################
  
  [-] COMMENT:

	ADateTime 으로 지정한 시간의 데이터를 최초 검색한다.
	최초 검색후에는 DVRSearchNext 로 이전/다음 프레임을 검색할 수 있다.
  
  [-] PARAMETERS:
    
	SearchHandle
	  - Handle from DVRSearchOpen()
	ADateTime
	  - 검색하고자 하는 날짜와 시간을 지정한다.
	lpSearchResult
	  - 검색이 성공했을 경우에 검색 결과를 돌려준다.
	    이 값은 NULL 일 수 없다.
	    

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrSearchNoFile
	DVRErrNoExistConnection

###############################################################################
*/

DVRRESULT WINAPI DVRSearchFirst( 
	DVRSEARCHHANDLE SearchHandle, 
	LPSYSTEMTIME lpDateTime,
	BOOL bBackward,
	LPDVRSEARCHRESULT lpSearchResult 
);

/*
###############################################################################
  
  [-] COMMENT:

	DVRSearchFirst 로 검색된 데이터의 이전/다음 프레임을 검색한다.	
  
  [-] PARAMETERS:
    
	SearchHandle
	  - Handle from DVRSearchOpen()
	bBackward
	  - 이값이 TRUE 면 저장된 이전 프레임을 검색하고 
	    FALSE 이면 다음 프레임을 검색한다.
	Skip
	  - 건너뛸 프레임수을 지정한다.
	    

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrSearchNoFile
	DVRErrNoExistConnection

###############################################################################
*/

DVRRESULT WINAPI DVRSearchNext( 
	DVRSEARCHHANDLE SearchHandle, 
	BOOL bBackward,
	INT Skip,
	LPDVRSEARCHRESULT lpSearchResult 
);

/*
###############################################################################
  
  [-] COMMENT:

	ADateTime 으로 지정한 시간의 데이터를 최초 검색한다.
	최초 검색후에는 DVRSearchNext 로 이전/다음 프레임을 검색할 수 있다.
  
  [-] PARAMETERS:
    
	SearchHandle
	  - Handle from DVRSearchOpen()
	ADateTime
	  - 검색하고자 하는 날짜와 시간을 지정한다.
	lpSearchResult
	  - 검색이 성공했을 경우에 검색 결과를 돌려준다.
	    이 값은 NULL 일 수 없다.
	    

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrSearchNoFile
	DVRErrNoExistConnection

###############################################################################
*/

DVRRESULT WINAPI DVRSearchFirstEx( 
	DVRSEARCHHANDLE SearchHandle, 
	LPDVRSEARCHOPTION lpSearchOption,
	BOOL bBackward,
	LPDVRSEARCHRESULTEX lpSearchResult 
);

/*
###############################################################################
  
  [-] COMMENT:

	DVRSearchFirst 로 검색된 데이터의 이전/다음 프레임을 검색한다.	
  
  [-] PARAMETERS:
    
	SearchHandle
	  - Handle from DVRSearchOpen()
	bBackward
	  - 이값이 TRUE 면 저장된 이전 프레임을 검색하고 
	    FALSE 이면 다음 프레임을 검색한다.
	Skip
	  - 건너뛸 프레임수을 지정한다.
	    

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrSearchNoFile
	DVRErrNoExistConnection

###############################################################################
*/

DVRRESULT WINAPI DVRSearchNextEx( 
	DVRSEARCHHANDLE SearchHandle, 
	BOOL bBackward,
	INT Skip,
	LPDVRSEARCHRESULTEX lpSearchResult 
);

DVRRESULT WINAPI DVRSearchFirstAudio(
	DVRSEARCHHANDLE SearchHandle,
	INT AChannel,
	LPSYSTEMTIME lpDateTime,
	LPDVRSEARCHAUDIORESULT lpSearchAudioResult
);

DVRRESULT WINAPI DVRSearchNextAudio(
	DVRSEARCHHANDLE SearchHandle,
	LPDVRSEARCHAUDIORESULT lpSearchAudioResult
);

DVRRESULT WINAPI DVRSearchGetAudioData(
	DVRSEARCHHANDLE SearchHandle,
	LPVOID lpAudioBuffer
);

typedef INT DVRCLIPSEARCHSTATUS, *LPDVRCLIPSEARCHSTATUS;

#define	CLIPSEARCHSTATUS_SOCKETERROR	DVRCLIPSEARCHSTATUS(-3)
#define CLIPSEARCHSTATUS_INVALIDDATA	DVRCLIPSEARCHSTATUS(-2)
#define CLIPSEARCHSTATUS_FAIL			DVRCLIPSEARCHSTATUS(-1)
#define CLIPSEARCHSTATUS_SEARCHING		DVRCLIPSEARCHSTATUS(0)
#define CLIPSEARCHSTATUS_FINISH			DVRCLIPSEARCHSTATUS(1)
#define CLIPSEARCHSTATUS_FIND			DVRCLIPSEARCHSTATUS(2)
#define CLIPSEARCHSTATUS_FORMATTING		DVRCLIPSEARCHSTATUS(3)

typedef struct tagDVRIndexClipData
{
    INT				nID;			// Index Clip ID
    INT				nFrameRate;		// Max Frame Rate
    INT				nEventDwell;    // (PreAlarm + Event) time
    DVREVENTTYPE	nEventType;		// Event Type
    INT				nTotalFrames;   // PreAlarm # + Event #
    SYSTEMTIME		tPreBegin;		// PreAlarm Begin
    SYSTEMTIME		tPreEnd;		// PreAlarm End
    INT				nEventNum;		// nTotalFrames - PreAlarm #
    SYSTEMTIME		tEventBegin;	// PostAlarm Begin
    SYSTEMTIME		tEventEnd;		// PostAlarm End
    INT				nReserved;
}
DVRINDEXCLIPDATA, *LPDVRINDEXCLIPDATA;

DVRRESULT WINAPI DVRSearchClipFirst( 
	DVRSEARCHHANDLE			SearchHandle,
	LPSYSTEMTIME			StartTime, 
	LPSYSTEMTIME			EndTime,
	DVREVENTTYPE			VideoType,	// EVENTTYPE_SENSOR, EVENTYPE_MOTION, EVENTYPE_INSTANT, EVENTTYPE_ALL	
    LPDVRINDEXCLIPDATA		ClipResult,
	LPDVRCLIPSEARCHSTATUS	ASearchStatus 
);

DVRRESULT WINAPI DVRSearchClipNext(  
	DVRSEARCHHANDLE			SearchHandle,
	LPDVRINDEXCLIPDATA		ClipResult,
	LPDVRCLIPSEARCHSTATUS	ASearchStatus 
);

DVRRESULT WINAPI DVRSearchClipData(  
	DVRSEARCHHANDLE			SearchHandle,
	INT						nClipID,
	INT						nSMIndex,
    LPDVRSEARCHRESULT		SearchResult 
);

DVRRESULT WINAPI DVRSearchGetImage( 
	DVRSEARCHHANDLE SearchHandle, 
	INT nCamera,
	LPVOID lpBuffer,
	INT nBufferSize
);

DVRRESULT WINAPI DVRSearchGetImageJPEG( 
	DVRSEARCHHANDLE SearchHandle, 
	INT nCamera,
	LPVOID lpBuffer,
	INT * nBufferSize
);

/*
###############################################################################
  
  [-] COMMENT:
    
	It displays received frame onto reserved DC area.
  
  [-] PARAMETERS:
    
	SearchHandle 
	  - Handle from DVRSearchOpen()
	nCamera
	  - Camera number to be displayed (1.. Max Camera)
	DestDC
	  - Destination of Device Context Handle that image to be displayed.
	DestRect
	  - Target region (rectangle) of DC that image to be displayed.

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrOperationFail
	DVRErrInvalidParam
	DVRErrNoExistConnection
 
###############################################################################
*/

DVRRESULT WINAPI DVRSearchDrawImage( 
	DVRSEARCHHANDLE SearchHandle, 
	INT nCamera,
	HDC DestDC,
	LPRECT lpDestRect
);

/*DVRRESULT WINAPI DVRSearchDrawImage_VB( 
	HANDLE SearchHandle, 
	INT nCamera,
	HDC DestDC,
	LPRECT DestRect
);*/


/*
###############################################################################
  
  [-] COMMENT:
    
	DVR 에 Pan/Tilt 사용 권한을 획득한다.
	다른 사용자가 사용중이거나 Pan/Tilt 를 사용할 수 없을 경우
	DVRErrOperationFail 을 반환한다.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrOperationFail
	DVRErrNoExistConnection
 
###############################################################################
*/

DVRRESULT WINAPI DVRPanTiltLock( 
	DVRHANDLE Handle 
);

/*
###############################################################################
  
  [-] COMMENT:
    
	DVR 에 Pan/Tilt 사용 권한을 반환한다.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrOperationFail
	DVRErrNoExistConnection
 
###############################################################################
*/


DVRRESULT WINAPI DVRPanTiltUnLock( 
	DVRHANDLE Handle 
);


/*
###############################################################################
  
  [-] COMMENT:
    
     bEnable을 TRUE 로 설정하면 비동기로 PTZ 가 제어되고, PTZ 명령을 자동으로 반복하여 보낼 수 있습니다. 
	 nAutoRepeatInterval은 비동기 제어모드일 때, 반복하여 명령을 전송하는 주기를 나타냅니다.
     명령은 반복하여 보내는 이유는, 카메라 특성에 따라 Stop 명령이 날아오기 전까지 Pan/Tilt가 멈추지 않는 카메라가 있고, 
	 어떤 카메라는 일정 거리만큼 이동 후 자동으로 멈추는 카메라가 있기 때문에 동일하게 동작하기 위하여 
	 반복적으로 같은 명령을 전송하는 것입니다.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()
	bEnable
	  - 비동기 제어모드를 Enable 합니다.
	nAutoRepeatInterval
	  - 비동기 제어시 명령을 반복하여 전송할 시간간격(밀리세컨드)입니다.
        이 값이 '0' 이면 명령을 반복하여 전송하지 않습니다.
        반복적으로 전송되는 명령은 Pan/Tilt/ZoomIn/ZoomOut 명령입니다

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrInvalidParam
	DVRErrOperationFail
	DVRErrNoExistConnection
 
###############################################################################
*/


DVRRESULT WINAPI DVRPanTiltEnableAsyncControl( 
	DVRHANDLE Handle, 
	BOOL bEnable,
	UINT nAutoRepeatInterval
);

/*
###############################################################################
  
  [-] COMMENT:
    
	Pan/Tilt 제어를 할 카메라를 선택한다.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()
	nCamera
	  - Pan/Tilt 로 제어할 카메라의 번호를 지정한다.

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrInvalidParam
	DVRErrOperationFail
	DVRErrNoExistConnection
 
###############################################################################
*/

DVRRESULT WINAPI DVRPanTiltCameraSelect( 
	DVRHANDLE Handle, 
	INT nCamera
);

/*
###############################################################################
  
  [-] COMMENT:
    
	Pan/Tilt Speed Control.
	This function supported only DVR system.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVRPanTiltSetSpeed()
	dwSpeedCommand
	  - PTZ_SPEED_XXXX 에 대한 비트 마스크.
	nCamera
	  - Pan/Tilt 로 제어할 카메라의 번호를 지정한다. ( 1 Base )
        0을 조정할 경우 전체 카메라의 Speed 값을 조정한다.
	nSpeed
	  - 1..7 사이의 값 

  [-] RETURN VALUES:

    DVRErrOK
    DVRInvalidHandle
	DVRErrInvalidParam
	DVRErrOperationFail
	DVRErrNoExistConnection
	DVRErrNotImplemented
 
###############################################################################
*/


#define PTZ_SPEED_PANMOVE   0x01
#define PTZ_SPEED_TILTMOVE  0x02
#define PTZ_SPEED_ZOOMIN    0x04
#define PTZ_SPEED_ZOOMOUT   0x08
#define PTZ_SPEED_FOCUSIN   0x10
#define PTZ_SPEED_FOCUSOUT  0x20
#define PTZ_SPEED_ALLVALUE  0xFF

DVRRESULT WINAPI DVRPanTiltSetSpeed( 
	DVRHANDLE Handle, 
	DWORD dwSpeedCommand, 
	int nCamera, 
	int nSpeed);

#define PTZ_STOP			0
#define PTZ_MOVE_UP			1
#define PTZ_MOVE_DOWN		2
#define PTZ_MOVE_LEFT		3
#define	PTZ_MOVE_RIGHT		4
#define PTZ_MOVE_UPRIGHT    5
#define PTZ_MOVE_UPLEFT     6
#define PTZ_MOVE_DOWNRIGHT  7
#define PTZ_MOVE_DOWNLEFT   8

DVRRESULT WINAPI DVRPanTiltMove( 
	DVRHANDLE Handle, 
	INT nDirection,			// PTZ_MOVEUP, PTZ_MOVEDOWN, PTZ_MOVELEFT, PTZ_MOVERIGHT
	INT nSpeed				// Pan/Tilt Speed
);

DVRRESULT WINAPI DVRPanTiltMovePreset( 
	DVRHANDLE Handle, 
	INT nPreset				// 1 Base
);

DVRRESULT WINAPI DVRPanTiltSetPreset( 
	DVRHANDLE Handle, 
	INT nPreset
);

DVRRESULT WINAPI DVRPanTiltClearPreset( 
	DVRHANDLE Handle, 
	INT nPreset
);

DVRRESULT WINAPI DVRPanTiltZoomIn( DVRHANDLE Handle );

DVRRESULT WINAPI DVRPanTiltZoomOut( DVRHANDLE Handle );

DVRRESULT WINAPI DVRPanTiltFocusIn( DVRHANDLE Handle );

DVRRESULT WINAPI DVRPanTiltFocusOut( DVRHANDLE Handle );

DVRRESULT WINAPI DVRPanTiltIrisOpen( DVRHANDLE Handle );

DVRRESULT WINAPI DVRPanTiltIrisClose( DVRHANDLE Handle );

DVRRESULT WINAPI DVRPanTiltAuxOnOff( DVRHANDLE Handle, int nAuxNo, BOOL bOnOff );

DVRRESULT WINAPI DVRPanTiltControlWiper( DVRHANDLE Handle, int nCamera );

DVRRESULT WINAPI DVRPanTiltControlLight( DVRHANDLE Handle, int nCamera );

DVRRESULT WINAPI DVRPanTiltControlTour( DVRHANDLE Handle, int nCamera );

DVRRESULT WINAPI DVRPanTiltControlWiper2( DVRHANDLE Handle, int nOnOff );

DVRRESULT WINAPI DVRPanTiltControlLight2( DVRHANDLE Handle, int nOnOff );
/*
###############################################################################
  
  [-] COMMENT:
    
	DVR에서 확장 이벤트가 발생하면 그 발생한 정보를 가져온다.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()
	lpDVRTrans
	  - Transaction 에 관한 가져올 정보.

  [-] RETURN VALUES:

  DVRErrOK
	DVRErrInvalidHandle
  DVRErrInvalidParam
	DVRErrNoExistConnection
 
###############################################################################
*/

DVRRESULT WINAPI DVRGetAlarmInfo( 
	DVRHANDLE Handle, 
	LPDVRTRANSACTION lpDVRTrans
);


/*
###############################################################################
  
  [-] COMMENT:
    
	 Transaction에서 발생한 ID를 보내준다.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()
	pTID
	  - 발생한 Transaction ID.
	lpDVRTrans 
	  - Transaction 에 관한 정보. 해당 Transaction ID에 관한 정보, 인자를 주지 않으면
	  	이벤트가 발생한 그 순간의 Transaciton 정보.

  [-] RETURN VALUES:

  DVRErrOK
	DVRErrInvalidHandle  
	DVRErrNoExistConnection
 
###############################################################################
*/

DVRRESULT WINAPI DVRSetTransactionID(
	DVRHANDLE Handle,
	char *pTID, 
	LPDVRTRANSACTION lpDVRTrans
);


/*
###############################################################################
  
  [-] COMMENT:
    
	 Transaction이 발생한 ID와 발생시간을 보내주면 그 해당 이미지를 보여준다.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVRSearchOpen()
	lpTIDSearch
	  - 찾고자하는 Transaction ID 와 그 발생시간.
	lpSearchResult 
	  - lpTIDSearch 에 의한 결과를 받아오는 인자.[이미지, 이미지관련사항 등] 

  [-] RETURN VALUES:

  DVRErrOK
	DVRErrInvalidHandle
	DVRErrSearchNoFile  
	DVRErrNoExistConnection
 
###############################################################################
*/

DVRRESULT WINAPI DVRSearchTransactionID( 
	DVRSEARCHHANDLE SearchHandle, 
	LPTIDSEARCH lpTIDSearch, 
	LPDVRSEARCHRESULT lpSearchResult
);



/*
###############################################################################
  
  [-] COMMENT:
    
	 DVRDVRSearchTransactionID의 결과로 받은 이미지의 가장 첫 화면을 가져온다. 
	 즉, 발생된 이벤트의 처음 화면이 보여진다.
	 DVRDVRSearchTransactionID, DVRSearchFirst등이 이루어진 이후에 사용해야한다.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVRSearchOpen()

	lpSearchResult 
	  - Search 결과를 받아오는 인자.[이미지, 이미지관련사항 등] 

  [-] RETURN VALUES:

  DVRErrOK
	DVRErrInvalidHandle
	DVRErrSearchNoFile
	DVRErrSocketError
	DVRErrOperationFail  
	DVRErrNoExistConnection
 
###############################################################################
*/

DVRRESULT WINAPI DVRSearchStartOfEvent( 
	DVRSEARCHHANDLE SearchHandle, 
	LPDVRSEARCHRESULT lpSearchResult
);

/*
###############################################################################
  
  [-] COMMENT:
    
	 DVRSearchTransactionID의 결과로 받은 이미지의 가장 마지막 화면을 가져온다. 
	 즉, 발생된 이벤트의 마지막 화면이 보여진다.
	 DVRDVRSearchTransactionID, DVRSearchFirst등이 이루어진 이후에 사용해야한다.
  
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVRSearchOpen()

	lpSearchResult 
	  - Search 결과를 받아오는 인자.[이미지, 이미지관련사항 등] 

  [-] RETURN VALUES:

  DVRErrOK
	DVRErrInvalidHandle
	DVRErrSearchNoFile
	DVRErrSocketError
	DVRErrOperationFail  
	DVRErrNoExistConnection	
 
###############################################################################
*/

DVRRESULT WINAPI DVRSearchEndOfEvent( 
	DVRSEARCHHANDLE SearchHandle, 
	LPDVRSEARCHRESULT lpSearchResult 
);
/*
###############################################################################
  
  [-] COMMENT:
     Setup을 시작하면서 현재 다른 사용자가 Setup에 접속되어있는지 확인한다.
	
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()

  [-] RETURN VALUES:

	DVRErrOK
	DVRErrInvalidHandle
	DVRErrNoExistConnection
	DVRErrSetupOperationFail

###############################################################################
*/
DVRRESULT WINAPI DVRSetupStart(
	DVRHANDLE Handle
);

/*
###############################################################################
  
  [-] COMMENT:
     
	
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()

	
  [-] RETURN VALUES:

	DVRErrOK
	DVRErrInvalidHandle
	DVRErrNoExistConnection
	DVRErrSetupOperationFail

###############################################################################
*/
DVRRESULT WINAPI DVRSetupEnd(
	DVRHANDLE Handle
);

/*
###############################################################################
  
  [-] COMMENT:
     
	
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()

  [-] RETURN VALUES:

	DVRErrOK
	DVRErrInvalidHandle
	DVRErrNoExistConnection
	DVRErrSetupOperationFail

###############################################################################
*/
DVRRESULT WINAPI DVRSetupApply(
	DVRHANDLE Handle
);

/*
###############################################################################
  
  [-] COMMENT:
     
	
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()

  [-] RETURN VALUES:

	DVRErrOK
	DVRErrInvalidHandle
	DVRErrNoExistConnection
	DVRErrSetupOperationFail

###############################################################################
*/

#define   FRAMESETTING_EFFECT_OTHERCHANNELCHANGE     0x01
#define   FRAMESETTING_EFFECT_FRAMERANGECHANGE       0x02

typedef enum {
    DVRSetupKind_PositionName = 10,
    DVRSetupKind_FrameInfo    = 20
}DVRSetupKind;

typedef struct {
    DWORD   cbSize;
    int nCameraNo;
}
SetupArg_PositionName,*LPSetupArg_PositionName;

typedef struct {
    DWORD cbSize;
    char szPositionName[40];
}
SetupValue_PositionName,*LPSetupValue_PositionName;

typedef struct {
    DWORD   cbSize;
    int		nCameraNo;
	DWORD	dwSettingEffect;
}
SetupArg_FrameInfo,*LPSetupArg_FrameInfo;

typedef struct {
    DWORD cbSize;
    DWORD nFPS;
    DWORD nSaveFPS;
    DWORD nResolution;
    DWORD nQuality;
    DWORD nSesitivity;
	  DWORD nCodec;
//	DVRVIDEORESOLUTION	nResolution;
//  DVRVIDEOQUALITY		nQuality;
}
SetupValue_FrameInfo,*LPSetupValue_FrameInfo;


DVRRESULT WINAPI DVRSetupGet(
	DVRHANDLE Handle,
	DVRSetupKind nSetupKind,
	void *inParam, 
	void *outParam
);

DVRRESULT WINAPI DVRSetupSet(
	DVRHANDLE Handle,
	DVRSetupKind nSetupKind,
	void *inParam, 
	void *outParam
);

/*
###############################################################################
  
  [-] COMMENT:
     
	
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()

  [-] RETURN VALUES:

	DVRErrOK
	DVRErrInvalidHandle
	DVRErrNoExistConnection
	DVRErrSetupOperationFail

###############################################################################
*/

#define  FRAMEBOARDCAPS_SYNC_RESOLUTION         4

#define  FRAMECHANNELCAPS_CONTROL_CAPTURESPEED  1
#define  FRAMECHANNELCAPS_CONTROL_SAVESPEED     2
#define  FRAMECHANNELCAPS_CONTROL_RESOLUTION    4
#define  FRAMECHANNELCAPS_CONTROL_QUALITY       8
#define  FRAMECHANNELCAPS_CONTROL_SENSITIVITY   16

#define  BOARDTYPE_1000      1000
#define  BOARDTYPE_2000      2000
#define  BOARDTYPE_3000      3000
#define  BOARDTYPE_3100      3100  
#define  BOARDTYPE_7000      7000
#define  BOARDTYPE_7100      7100
#define  BOARDTYPE_9012      9012
#define  BOARDTYPE_9016      9016

typedef enum {
    SetupQryCmd_FrameCaps,
    SetupQryCmd_LogExportCaps
}SetupQryCommand;

typedef struct {
    DWORD	cbSize;
	DWORD	dwBoardCaps;
    DWORD	dwChannelCaps;
	int		nBoardType;        
	int		nBoardMaxFrame;   
	int		nChannelMaxFrame;
	int		nVideoFormat;     
	BYTE	FrameSpeedValues[32];
	BOOL	ResolutionValues[8];
	BYTE	SensitivityValues[32];
	char	szCodecList[20][16];
}
SetupQryResult_FrameCaps,*LPSetupQryResult_FrameCaps;

#define  SUPPORTLOGCAPS_SYSTEMLOG 1
#define  SUPPORTLOGCAPS_EVENTLOG  2
#define  SUPPORTLOGCAPS_ALARMLOG  4
#define  SUPPORTLOGCAPS_DRIVELOG  8

typedef struct {
    DWORD	cbSize;
	DWORD	dwSupportLogCaps;
}
SetupQryResult_LogExportCaps,*LPSetupQryResult_LogExportCaps;

DVRRESULT WINAPI DVRSetupQuery(
	DVRHANDLE Handle,
	SetupQryCommand nQueryCmd,
	void *inParam, 
	void *outParam
);

/*
###############################################################################
  
  [-] COMMENT:
     
	
  [-] PARAMETERS:
    
	Handle 
	  - Handle from DVROpenConnection()

  [-] RETURN VALUES:

	DVRErrOK
	DVRErrInvalidHandle
	DVRErrNoExistConnection
	DVRErrSetupOperationFail

###############################################################################
*/

typedef enum {
    LogExport_Open,
    LogExport_Read,
  	LogExport_Close
}LogExportCommand;

typedef struct {
    DWORD		cbSize;
    int			nLogType;		// System Log-0; Event Log-1; Alarm Log-2; Drive Log-3;
	SYSTEMTIME	stExportDate;
}
LogExportOpenParam,*LPLogExportOpenParam;

typedef struct {
    DWORD	cbSize;
    int		nLogRecCount;		
	int		nLogVersion;	
}
LogExportOpenResult,*LPLogExportOpenResult;

typedef struct {
    DWORD	cbSize;
    int		nRequestCount;		
}
LogExportReadParam,*LPLogExportReadParam;

typedef struct {
    DWORD	cbSize;
    int		nLogRecNo;	
	char*	lpLogBuffer;
	int		dwLogSize;
}
LogExportReadResult,*LPLogExportReadResult;

DVRRESULT WINAPI DVRLogExport(
	DVRHANDLE Handle,
	LogExportCommand LogExportCommand,
	void *lpLogExportParam, 
	void *lpLogExportResult
);


/*
###############################################################################

  [-] COMMENT:
     Main에서 LiveDisplay한다. 
 
 
  [-] PARAMETERS:
    
	Handle 
		- Handle from DVROpenConnection()
    CameraNo
		- 카메라번호
	DestRect
		- 그려질 크기 
	WindowHandle
		- 그려질 윈도우
	DisplayID
		- 해당 구조체정보의 아이디 : 내부에서 채워지는 값. 
    DrawOpt
		- 그리는 옵션,  1 : OpenAPI함수에서 자체내에서 그림 
						0 : 개발자가 어느 시기에 그리겠다고 결정 한다. ex) OnPanit 이벤트 등

  [-] RETURN VALUES:

	DVRErrOK
	DVRErrInvalidHandle
	DVRErrNoExistConnection
	  
###############################################################################
*/

DVRRESULT WINAPI DVRStartLiveDisplay (
	DVRHANDLE Handle,
    INT CameraNo,
	LPRECT DestRect,
	DVRHANDLE WindowHandle,
	LPDWORD DisplayID,
    BYTE DrawOpt
);

/*
###############################################################################

  [-] COMMENT:
     Main에서 LiveDisplay를 Stop한다. 
 
 
  [-] PARAMETERS:
    
	Handle 
		- Handle from DVROpenConnection()
	
	DisplayID
		- 플레이되고 있는 DisplayID
		
  [-] RETURN VALUES:

	DVRErrOK
	DVRErrInvalidHandle
	DVRErrNoExistConnection
	  
###############################################################################
*/

DVRRESULT WINAPI DVRStopLiveDisplay (
	DVRHANDLE Handle,
	DWORD DisplayID
);


// deprecated from OpenAPI 2.5.5.5
typedef struct {
	WORD width, height;
	BYTE reserved[28];
} Resolution;


enum eRecordSaveMode
{
   SaveMode_NORECORDING,       // 저장하지 않는다
   SaveMode_CONTINUOUS,        // 연속 저장 모드
   SaveMode_MOTION,            // 모션이 걸렸을때만 저장함
   SaveMode_SENSOR,            // 센서가 걸렸을때만 저장함
   SaveMode_INSTANT,           // 수동으로 저장이 요청됨
   SaveMode_INTENSIVE,         // 캡쳐 프레임수를 일시적으로 많게함
   SaveMode_ACCESSCONTROL,     // AccessControl 연동
   SaveMode_ATM,
   SaveMode_NONE = 0xFF         // 저장 여부 통보를 지원 하지 않는다.
};

// Use this struct instead of Resolution struct
typedef struct {
	WORD width, height;
	eRecordSaveMode RemoteRecordingStatus;
	eRecordSaveMode CaptureRecordingStatus;
	BYTE reserved[26];
} LiveInfo;

#define ALARMTYPE_MOTION     	2
#define ALARMTYPE_SENSOR     	3

typedef struct {
	char			SiteIP[40];
	SYSTEMTIME		KeyDate;
	int				KeyIndex;
	int				ImageIndex;
	PBYTE			JpegBuffer;
	int				JpegSize;
	int				JpegPosition;	// reserved for later

	// valid data if ImageIndex == 0
	int				AlarmType;
	char			SiteCode[40];
	char			SiteName[40];
	SYSTEMTIME		EventOnTime;
	SYSTEMTIME		EventOffTime;
	int				CameraIndex;
	int				SensorIndex;
	char			CameraName[52];
	char			FileName[256];
} 
DVRAlarmData, *LPDVRAlarmData;

DVRRESULT WINAPI DVRAlarmMonitorStart (
	DWORD		dwPort,
	char *		szSavePath,
	char *		szDBPath,
	DWORD		dwAlarmCallBack,
	DWORD		dwAlarmCallBackInstance
);

DVRRESULT WINAPI DVRAlarmMonitorStop();

//AudioChatRecvCallback = (*

/*
###############################################################################

  [-] COMMENT:
     Voice Chatting support for PC-DVR. 
 	  
###############################################################################
*/

#define  DVRVOICECHAT_EVENT_RECVDATA		1
#define  DVRVOICECHAT_EVENT_NOTIFY			2
#define		DVRVOICECHAT_NOTIFY_DISCONNECTED	1	

// Support the PCM Capture from security board or hardware
// Support the PCM Capture data callback to application for capture board audio on server
#define  DVRVOICECHATCAPS1_SUPPORT_CAPTURE_BOARD 0x00000001

// Support the PCM Capture from SoundCard on server
// Support the PCM Capture data callback to application for Sound Card on server
#define  DVRVOICECHATCAPS1_SUPPORT_CAPTURE_SERVER 0x00000002

// Support the PCM Capture from assoisiated audio to Camera(Analor or IP Camera) on server
// Support the PCM Capture data callback to application for assoisiated audio to Camera(Analor or IP Camera) of server
#define  DVRVOICECHATCAPS1_SUPPORT_CAPTURE_CAMERA 0x00000004


// Support the listen to audio from security board or hardware
// Support the play in API for capture board audio on server
#define  DVRVOICECHATCAPS1_SUPPORT_LISTEN_BOARD 0x00000010
         
// Support the listen to audio from Sound Card on server
// Support the play in API for Sound Card on server
#define  DVRVOICECHATCAPS1_SUPPORT_LISTEN_SERVER 0x00000020
         
// Support the listen to audio from assoisiated audio to Camera(Analor or IP Camera) on server
// Support the play in API for assoisiated audio to Camera(Analor or IP Camera) of server
#define  DVRVOICECHATCAPS1_SUPPORT_LISTEN_CAMERA 0x00000040


// Support the talk to assosiated audio to capture board on server by automatic(Send audio internally by API).
#define  DVRVOICECHATCAPS1_SUPPORT_TALK_BOARD 0x00000100
         
// Support the talk to assosiated audio to SoundCard on server by automatic(Send audio internally by API).
#define  DVRVOICECHATCAPS1_SUPPORT_TALK_SERVER 0x00000200

// Support the talk to assosiated audio to Camera(Analor or IP Camera) on server by automatic(Send audio internally by API).
#define  DVRVOICECHATCAPS1_SUPPORT_TALK_CAMERA 0x00000400



// Support the send PCM Data to assosiated audio to capture board on server by user.
#define  DVRVOICECHATCAPS1_SUPPORT_SEND_BOARD 0x00001000
         
// Support the send PCM Data to assosiated audio to SoundCard on server by user.
#define  DVRVOICECHATCAPS1_SUPPORT_SEND_SERVER 0x00002000
         
// Support the send PCM Data to assosiated audio to Camera(Analor or IP Camera) on server by user.
#define  DVRVOICECHATCAPS1_SUPPORT_SEND_CAMERA 0x00004000

#define  DVRVOICECHATCAPS1_SUPPORT_MULTIPLECAPTURE_BOARD 0x00010000

#define  DVRVOICECHATTARGET_BOARD   1
#define  DVRVOICECHATTARGET_SERVER  2
#define  DVRVOICECHATTARGET_CAMERA  3

typedef struct {
	INT				nTargetType;
	INT				nTargetAudioNo;
	INT				nTargetCameraNo;
}
DVRVoiceTarget;

typedef struct {  
    DWORD			cbSize;
	SYSTEMTIME		ServerTime;
	SYSTEMTIME		LocalTime;
	BOOL			bDaylightSavingTime;    
	DVRVoiceTarget	CaptureTarget;
    LPVOID			lpAudioData;
    DWORD			dwAudioSize;
	WAVEFORMATEX	CaptureWaveFormat;
}
DVRVoiceChatRecvData, *LPDVRVoiceChatRecvData;

typedef struct {
	DWORD			cbSize;
	INT				NotifyID;
	INT				NotifyParam1;
	INT				NotifyParam2;
}
DVRVoiceChatNotify, *LPDVRVoiceChatNotify;

typedef void (WINAPI *LPVoiceChatRecvCallback)(HANDLE Handle, INT AudioEventID, LPVOID AudioEventData, DWORD dwUserInstance);

#define	IN
#define	OUT

typedef struct {
    IN  DWORD			cbSize;
    IN  DWORD			dwCallback;
    IN  DWORD			dwUserInstance;
    IN	DWORD			dwOpenFlag;
    IN  CHAR			szExternalModulePath[256];
    IN  BYTE			Dummy1[32];
    OUT INT				nAudioChannelCount;
	OUT DWORD			dwCaps1;
    OUT	DWORD			dwCaps2;
    OUT WAVEFORMATEX	TalkWaveFormat;
	OUT	BYTE			Dummy2[98];
    
}
DVRVoiceChatOpenParam, *LPDVRVoiceChatOpenParam;

typedef struct {
    IN	DWORD			cbSize;
	IN	DVRVoiceTarget	CaptureTarget;    
    	DWORD			dwReserved1;
    	DWORD			dwReserved2;
    	DWORD			dwReserved3;
    	DWORD			dwReserved4;
}
DVRVoiceCaptureStartParam, *LPDVRVoiceCaptureStartParam;

typedef struct {
    IN	DWORD			cbSize;
	IN	DVRVoiceTarget	CaptureTarget;    
    	DWORD			dwReserved1;
    	DWORD			dwReserved2;
    	DWORD			dwReserved3;
    	DWORD			dwReserved4;
}
DVRVoiceCaptureStopParam, *LPDVRVoiceCaptureStopParam;

typedef struct {
    IN	DWORD			cbSize;
	IN	DVRVoiceTarget	ListenTarget;    
    	DWORD			dwReserved1;
    	DWORD			dwReserved2;
    	DWORD			dwReserved3;
    	DWORD			dwReserved4;
}
DVRVoiceListenStartParam, *LPDVRVoiceListenStartParam;

typedef struct {
    IN	DWORD			cbSize;
    	DWORD			dwReserved1;
    	DWORD			dwReserved2;
    	DWORD			dwReserved3;
    	DWORD			dwReserved4;
}
DVRVoiceListenStopParam, *LPDVRVoiceListenStopParam;

typedef struct {
    IN	DWORD			cbSize;
	IN	DVRVoiceTarget	TalkTarget;
    IN	LPVOID			lpPCMData;
    IN	DWORD			dwPCMSize;
		DWORD			dwReserved1;
		DWORD			dwReserved2;
		DWORD			dwReserved3;
		DWORD			dwReserved4;
}
DVRVoiceTalkSendParam, *LPDVRVoiceTalkSendParam;  

DVRRESULT WINAPI DVRVoiceChatOpen(DVRHANDLE Handle, LPDVRVoiceChatOpenParam lpstOpenParam);
DVRRESULT WINAPI DVRVoiceChatClose(DVRHANDLE Handle);
DVRRESULT WINAPI DVRVoiceCaptureStart(DVRHANDLE Handle, LPDVRVoiceCaptureStartParam lpStartParam);
DVRRESULT WINAPI DVRVoiceCaptureStop(DVRHANDLE Handle, LPDVRVoiceCaptureStopParam lpStopParam);
DVRRESULT WINAPI DVRVoiceListenStart(DVRHANDLE Handle, LPDVRVoiceListenStartParam lpStartParam);
DVRRESULT WINAPI DVRVoiceListenStop(DVRHANDLE Handle, LPDVRVoiceListenStopParam lpStopParam);
DVRRESULT WINAPI DVRVoiceTalkSend(DVRHANDLE Handle, LPDVRVoiceTalkSendParam lpSendParam);

DVRRESULT WINAPI DVRTwowayAudioStart(DVRHANDLE Handle, LPCSTR szExePath, HANDLE AppHandle);
DVRRESULT WINAPI DVRTwowayAudioStop(DVRHANDLE Handle);

/*
###############################################################################

  [-] COMMENT:
     접속된 서버로부터 IP 카메라가 연결된 채널을 알아온다. 
 
 
  [-] PARAMETERS:
    
	Handle 
		- Handle from DVROpenConnection()
	
	DisplayID
		- 플레이되고 있는 DisplayID
		
  [-] RETURN VALUES:

	DVRErrOK
	DVRErrInvalidHandle
	DVRErrInvalidParam
	  
###############################################################################
*/

typedef struct {
	DWORD dwArr[4];
} QuadDWORD, *LPQuadDWORD;

DVRRESULT WINAPI DVRGetHydChannelMask(
	DVRHANDLE Handle, 
	PDWORD pOutChnnelCount,
	LPQuadDWORD pOutChannelMask 
);

DVRRESULT WINAPI DVRHealthCheckRun(
     DVRHANDLE Handle,
     LPCSTR SiteIP,
     INT SitePort,
     LPCSTR UserID,
     LPCSTR Password,
	 LPSYSTEMTIME lpCheckFrom,
	 LPDWORD lpdwResultSize
);

DVRRESULT WINAPI DVRHealthCheckGetResult(
     DVRHANDLE Handle,
	 LPCSTR lpResultData,
     DWORD dwResultSize
);


#endif
