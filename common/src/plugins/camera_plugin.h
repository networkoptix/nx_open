#ifndef NX_CAMERA_PLUGIN_H
#define NX_CAMERA_PLUGIN_H

#include "plugin_api.h"


//!Network Optix Camera Integration Plugin API (c++)
/*!
    Contains structures and abstract classes to be implemented in real plugin.
    
    \par Following interfaces can be implemented by plugin:
    - \a nxcip::CameraDiscoveryManager. Mandatory. Used to discover camera and instanciate \a nxcip::BaseCameraManager
    - \a nxcip::BaseCameraManager. Mandatory. Used to get camera properties and get pointers to other interfaces
    - \a nxcip::CameraMediaEncoder. Optional. Used to get media stream from camera
    - \a nxcip::CameraRelayIOManager. Optional. Used to receive relay input port change state events and change relay output port state
    - \a nxcip::CameraPTZManager. Optional. Used for pan-tilt-zoom control

    \note all text values are NULL-terminated utf-8
    \note If not specified in interface's description, plugin interfaces are used in multithreaded environment the following way:\n
        - single interface pointer is never used concurrently by multiple threads, but different pointers to same interface 
            (e.g., \a nxcip::BaseCameraManager) can be used by different threads concurrently
*/
namespace nxcip
{
    static const int MAX_TEXT_LEN = 1024;

    //Error codes. Interface implementation MUST use these error codes when appropriate

    static const int NX_NO_ERROR = 0;
    static const int NX_NOT_AUTHORIZED = -1;
    static const int NX_INVALID_ENCODER_NUMBER = -2;
    static const int NX_UNKNOWN_PORT_NAME = -3;
    static const int NX_UNSUPPORTED_RESOLUTION = -9;
    static const int NX_UNDEFINED_BEHAVOUR = -20;
    static const int NX_NOT_IMPLEMENTED = -21;
    static const int NX_NETWORK_ERROR = -22;
    static const int NX_MORE_DATA = -23;
    static const int NX_OTHER_ERROR = -100;


    class BaseCameraManager;

    // {0D06134F-16D0-41c8-9752-A33E81FE9C75}
    static const nxpl::NX_GUID IID_CameraDiscoveryManager = { { 0x0d, 0x06, 0x13, 0x4f, 0x16, 0xd0, 0x41, 0xc8, 0x97, 0x52, 0xa3, 0x3e, 0x81, 0xfe, 0x9c, 0x75 } };

    //!Contains base camera information
    struct CameraInfo
    {
        //!Camera model name in any human readable format. MUST NOT be empty
        char modelName[256];
        //!Firmware version in any human readable format. Optional
        char firmware[256];
        //!Camera's unique identifier. MAC address can be used
        char uid[256];
        //!Camera management url
        char url[MAX_TEXT_LEN];
        //!Any data in implementation defined format (for internal plugin usage)
        char auxiliaryData[256];
        //!Plugin can specify default credentials to use with camera
        char defaultLogin[256];
        //!Plugin can specify default credentials to use with camera
        char defaultPassword[256];

        //!Initializes all values with zeros/empty strings
        CameraInfo()
        {
            modelName[0] = 0;
            firmware[0] = 0;
            uid[0] = 0;
            url[0] = 0;
            auxiliaryData[0] = 0;
            defaultLogin[0] = 0;
            defaultPassword[0] = 0;
        }
    };

    static const int CAMERA_INFO_ARRAY_SIZE = 1024;
    static const int MAX_MODEL_NAME_SIZE = 256;

    //!This interface is used to find cameras and create \a BaseCameraManager instance
    /*!
        Mediaserver has built-in UPNP & MDNS support, plugin needs only implement \a nxcip::CameraDiscoveryManager::fromUpnpData or 
        \a nxcip::CameraDiscoveryManager::fromMDNSData method.
        If camera do support neither UPNP nor MDNS, plugin can provide its own search method by implementing \a nxcip::CameraDiscoveryManager::findCameras

        In case camera is supported by multiple plugins, plugin to use is selected under following rules:\n
        - each received MDNS and UPNP response packet is first processed by plugins, then by built-in drivers. 
            Plugins are iterated in vendor name (\a nxcip::CameraDiscoveryManager::getVendorName) ascending order
        - it is undefined when \a CameraDiscoveryManager::findCameras is called (with regard to mdns and upnp search)
        - plugin can reserve model name(s) by implementing nxcip::CameraDiscoveryManager::getReservedModelList so that built-in drivers do not process cameras with that name

        \note Camera search methods MUST NOT take in account result of previously done searches
    */
    class CameraDiscoveryManager
    :
        public nxpl::PluginInterface
    {
    public:
        virtual ~CameraDiscoveryManager() {}

        //!Returns utf-8 camera vendor name
        /*!
            \param[out] buf Buffer of \a MAX_TEXT_LEN size
        */
        virtual void getVendorName( char* buf ) const = 0;

        //!Vendor-specific camera search method. Returns list of found cameras
        /*!
            It is recommended that this method works in asynchronous mode (only returning list of already found cameras).
            This method is called periodically.
            \param[out] cameras Array of size \a CAMERA_INFO_ARRAY_SIZE. Implementation filles this array with found camera(s) information
            \param[in] localInterfaceIPAddr String representation of local interface ip (ipv4 or ipv6). 
                If not NULL, camera search should be done on that interface only
            \return > 0 - number of found cameras, < 0 - on error. 0 - nothing found
        */
        virtual int findCameras( CameraInfo* cameras, const char* localInterfaceIPAddr ) = 0;
        //!Check host for camera presence
        /*!
            Plugin should investigate \a address for supported camera presence and fill \a cameras array with found camera(s) information
            This method is used to add camera with known ip (e.g., if multicast is disabled in network)
            \param[out] cameras Array of size \a CAMERA_INFO_ARRAY_SIZE
            \param[in] address String "host:port", port is optional
            \param[in] login Login to access \a address. If NULL, default login should be used
            \param[in] password Password to access \a address. If NULL, default password should be used
            \return > 0 - number of found cameras, < 0 - on error. 0 - nothing found
        */
        virtual int checkHostAddress( CameraInfo* cameras, const char* address, const char* login, const char* password ) = 0;
        //!MDNS camera search method
        /*!
            Mediaserver calls this method when it has found unknown MDNS host.

            If non-zero value is returned, \a cameraInfo MUST be filled by this method (it will be used later in call to \a nxcip::CameraDiscoveryManager::createCameraManager).
            \param[in] discoveryAddress Source address of \a mdnsResponsePacket (for ipv4 it is ip-address)
            \param[in] mdnsResponsePacket Received MDNS packet
            \param[in] mdnsResponsePacketSize Size of \a mdnsResponsePacket in bytes
            \param[out] cameraInfo If MDNS host is recognized, this structure is filled with camera parameters
            \return non zero, if MDNS data is recognized, 0 otherwise
        */
        virtual int fromMDNSData(
            const char* discoveryAddress,
            const unsigned char* mdnsResponsePacket,
            int mdnsResponsePacketSize,
            CameraInfo* cameraInfo ) = 0;
        //!UPNP camera search method
        /*!
            Mediaserver calls this method when it has found unknown UPNP device.

            If non-zero value is returned, \a cameraInfo MUST be filled by this method (it will be used later in call to \a nxcip::CameraDiscoveryManager::createCameraManager).
            \param[in] upnpXMLData Contains upnp data as specified in [UPnP Device Architecture 1.1, section 2.3]
            \param[in] upnpXMLDataSize Size of \a upnpXMLData in bytes
            \param[out] cameraInfo If upnp host is recognized, this structure is filled with camera data
            \return non zero, if upnp data is recognized and \a cameraInfo is filled with data, 0 otherwise
        */
        virtual int fromUpnpData( const char* upnpXMLData, int upnpXMLDataSize, CameraInfo* cameraInfo ) = 0;

        //!Instanciates camera manager instance based on \a info
        virtual BaseCameraManager* createCameraManager( const CameraInfo& info ) = 0;

        //!Get model model names, reserved by the plugin
        /*!
             \param[out] modelList        Array of \a char* buffers of size \a MAX_MODEL_NAME_SIZE where camera model names will be written. May be NULL.
             \param[in, out] count        A pointer to a variable that specifies the size of array pointed to by the \a modelList parameter. 
                                          When the function returns, this variable contains the number of model names copied to \a modelList.
                                          If the buffer specified by \a modelList parameter is not large enough to hold the data, 
                                          the function returns NX_MORE_DATA and stores the required buffer size in the variable pointed to by \a count. 
                                          In this case, the contents of the \a modelList array are undefined.
            \return \a NX_MORE_DATA if input buffer size is not sufficient
        */         
        virtual int getReservedModelList( char** modelList, int* count ) = 0;
    };


    //!Resolution of video stream picture
    struct Resolution
    {
        //!Width in pixels
        int width;
        //!Hidth in pixels
        int height;

        Resolution( int _width = 0, int _height = 0 )
        :
            width( _width ),
            height( _height )
        {
        }
    };

    //!Contains resolution and maximum fps, supported by camera for this resolution
    struct ResolutionInfo
    {
        //!Guess what
        Resolution resolution;
        //!Maximum fps, allowed for \a resolution
        float maxFps;

        ResolutionInfo()
        :
            maxFps( 0 )
        {
        }
    };

    // {528FD641-52BB-4f8b-B279-6C21FEF5A2BB}
    static const nxpl::NX_GUID IID_CameraMediaEncoder = { { 0x52, 0x8f, 0xd6, 0x41, 0x52, 0xbb, 0x4f, 0x8b, 0xb2, 0x79, 0x6c, 0x21, 0xfe, 0xf5, 0xa2, 0xbb } };

    static const int MAX_RESOLUTION_LIST_SIZE = 64;

    //!Provides encoder parameter configuration and media stream access (by providing media stream url)
    class CameraMediaEncoder
    :
        public nxpl::PluginInterface
    {
    public:
        virtual ~CameraMediaEncoder() {}

        //!Returns url of media stream as NULL-terminated utf-8 string
        /*!
            Returned url MUST consider stream parameters set with setResolution, setFps, etc...
            Supported protocols:\n
                - rtsp. RTP with h.264 and motion jpeg supported
                - http. Motion jpeg only supported
            \param[out] urlBuf Buffer of size \a MAX_TEXT_LEN. MUST be NULL-terminated after return
            \return 0 on success, otherwise - error code
        */
        virtual int getMediaUrl( char* urlBuf ) const = 0;

        //!Returns supported resolution list
        /*!
            \param[out] infoList Array of size \a MAX_RESOLUTION_LIST_SIZE
            \param[out] infoListCount Returned number of supported resolutions
            \return 0 on success, otherwise - error code
            \note Plugin is can return empty resolution list
        */
        virtual int getResolutionList( ResolutionInfo* infoList, int* infoListCount ) const = 0;

        //!Returns maximem bitrate in Kbps. 0 is interpreted as unlimited bitrate value
        /*!
            \param[out] maxBitrate Returned value of max bitrate
            \return 0 on success, otherwise - error code
        */
        virtual int getMaxBitrate( int* maxBitrate ) const = 0;

        //!Change resolution on specified encoder
        /*!
            \param[in] resolution
            \return 0 on success, otherwise - error code
        */
        virtual int setResolution( const Resolution& resolution ) = 0;

        /*!
            Camera is allowed to select fps different from requested, but it should try to choose fps nearest to requested
            \param[in] fps Requested fps
            \param[out] selectedFps *selectedFps MUST be set to actual fps implied
            \return 0 on success, otherwise - error code
        */
        virtual int setFps( const float& fps, float* selectedFps ) = 0;

        /*!
            Camera is allowed to select bitrate different from requested, but it should try to choose bitrate nearest to requested
            \param[in] bitrateKbps Requested bitrate in kbps
            \param[out] selectedBitrateKbps *selectedBitrateKbps MUST be set to actual bitrate implied
            \return 0 on success, otherwise - error code
        */
        virtual int setBitrate( int bitrateKbps, int* selectedBitrateKbps ) = 0;
    };


    class CameraPTZManager;
    class CameraMotionDataProvider;
    class CameraRelayIOManager;

        // {B7AA2FE8-7592-4459-A52F-B05E8E089AFE}
    static const nxpl::NX_GUID IID_BaseCameraManager = { { 0xb7, 0xaa, 0x2f, 0xe8, 0x75, 0x92, 0x44, 0x59, 0xa5, 0x2f, 0xb0, 0x5e, 0x8e, 0x8, 0x9a, 0xfe } };

    //!Provides base camera operations: getting/settings fps, resolution, bitrate, media stream url(s). Also provides pointer to other camera-management interfaces
    /*!
        Mediaserver uses this interface in following way:\n
            - requests encoder count with \a getEncoderCount and camera capabilities with \a getCameraCapabilities
            - requests each encoder params with \a getResolutionList, \a getMaxBitrate
            - analyzes camera capabilities and set selected encoder parameters with \a setResolution, \a setFps, \a setBitrate, \a setAudioEnabled
            - requests media url(s) with \a getMediaUrl
            - requests media stream(s) using provided url
    */
    class BaseCameraManager
    :
        public nxpl::PluginInterface
    {
    public:
        virtual ~BaseCameraManager() {}

        //!Provides maximum number of available encoders
        /*!
            E.g., if 2 means that camera supports dual-streaming, 3 - for tripple-streaming and so on.

            - encoder number starts with 0
            - encoders MUST be numbered in quality decrease order (0 - encoder with best quality, 1 - low-quality)

            \param[out] encoderCount Contains encoder count on return
            \return 0 on success, otherwise - error code
        */
        virtual int getEncoderCount( int* encoderCount ) const = 0;

        //!Returns encoder by index
        /*!
            Most likely will return same pointer on multiple requests with same \a encoderIndex

            \param[in] encoderIndex
            \param[out] encoderPtr
            \return \a NX_NO_ERROR on success, otherwise - error code:\n
                - \a NX_INVALID_ENCODER_NUMBER wrong \a encoderIndex value
            \note BaseCameraManager holds reference to \a CameraMediaEncoder
        */
        virtual int getEncoder( int encoderIndex, CameraMediaEncoder** encoderPtr ) = 0;

        //!Fills \a info struct with camera data
        /*!
            \param[out] info
            \return 0 on success, otherwise - error code
            \note This method can set some parameters that were navailable during discovery
        */
        virtual int getCameraInfo( CameraInfo* info ) const = 0;

        //!Enumeration of supported camera capabilities (bit flags)
        enum CameraCapability
        { 
            hardwareMotionCapability    = 0x01, //!< camera supports hardware motion. Plugin, returning this flag, MUST implement \a CameraMotionDataProvider interface
            relayInputCapability        = 0x02, //!< if this flag is enabled, \a CameraRelayIOManager MUST be implemented
            relayOutputCapability       = 0x04, //!< if this flag is enabled, \a CameraRelayIOManager MUST be implemented
            ptzCapability               = 0x08, //!< if this flag is enabled, \a CameraPTZManager MUST be implemented
            audioCapability             = 0x10, //!< if set, camera supports audio
            shareFpsCapability          = 0x20, //!< if second stream is running whatever fps it has => first stream can get maximumFps - secondstreamFps
            sharePixelsCapability       = 0x40, //!< if second stream is running whatever megapixel it has => first stream can get maxMegapixels - secondstreamPixels
            shareIpCapability           = 0x80  //!< allow multiple instances on a same IP address
        };
        //!Return bit set of camera capabilities (\a CameraCapability enumeration)
        /*!
            \param[out] capabilitiesMask
            \return 0 on success, otherwise - error code
        */
        virtual int getCameraCapabilities( unsigned int* capabilitiesMask ) const = 0;

        //!Set credentials for camera access
        virtual void setCredentials( const char* username, const char* password ) = 0;

        //!Turn on/off audio on ALL encoders
        /*!
            \param[in] audioEnabled If non-zero, audio should be enabled on ALL encoders, else - disabled
            \return 0 on success, otherwise - error code
        */
        virtual int setAudioEnabled( int audioEnabled ) = 0;

        //!MUST return not-NULL if \a ptzCapability is present
        /*!
            \note Increases \a CameraPTZManager instance reference counter
            \note Most likely will return same pointer on multiple requests
        */
        virtual CameraPTZManager* getPTZManager() const = 0;
        //!MUST return not-NULL if \a hardwareMotionCapability is present
        /*!
            \note Increases \a CameraMotionDataProvider instance reference counter
            \note Most likely will return same pointer on multiple requests
        */
        virtual CameraMotionDataProvider* getCameraMotionDataProvider() const = 0;
        //!MUST return not-NULL if \a relayInputCapability is present
        /*!
            \note Increases \a CameraRelayIOManager instance reference counter
            \note Most likely will return same pointer on multiple requests
        */
        virtual CameraRelayIOManager* getCameraRelayIOManager() const = 0;

        //!Returns text description of the last error
        /*!
            \param[out] errorString Buffer of size \a MAX_TEXT_LEN
        */
        virtual void getLastErrorString( char* errorString ) const = 0;
    };


    // {8BAB5BC7-BEFC-4629-921F-8390A29D8A16}
    static const nxpl::NX_GUID IID_CameraPTZManager = { { 0x8b, 0xab, 0x5b, 0xc7, 0xbe, 0xfc, 0x46, 0x29, 0x92, 0x1f, 0x83, 0x90, 0xa2, 0x9d, 0x8a, 0x16 } };

    //!Pan-tilt-zoom management
    /*!
        VMS client has several PTZ support levels, and the more features are supported
        by the camera's PTZ manager, the smoother will be the client's experience.

        These levels are as follows:
        - Basic PTZ. Manager must support continuous movement.
        - Basic PTZ with saved positions. Manager must additionally support absolute positioning in physical space.
        - Advanced PTZ. On this support level user can zoom in by selecting a region on an
          image from camera and can center the camera on an object by clicking on it.
          Manager must additionally support absolute positioning in logical space.

        Logical space for absolute positioning is defined as follows:
        - Pan is in degrees, from -180 to 180.
        - Tilt is in degrees, from -90 to 90. -90 corresponds to camera pointing straight down, 90 - straight up.
        - Zoom is set as width-based 35mm-equivalent focal lenght, see http://en.wikipedia.org/wiki/35_mm_equivalent_focal_length.
    */
    class CameraPTZManager
    :
        public nxpl::PluginInterface
    {
    public:
        virtual ~CameraPTZManager() {}

        enum Capability
        {
            ContinuousPanCapability             = 0x001,    //!< Camera supports continuous pan.
            ContinuousTiltCapability            = 0x002,    //!< Camera supports continuous tilt.
            ContinuousZoomCapability            = 0x004,    //!< Camera supports continuous zoom.
            
            AbsolutePanCapability               = 0x010,    //!< Camera supports absolute pan.
            AbsoluteTiltCapability              = 0x020,    //!< Camera supports absolute tilt.
            AbsoluteZoomCapability              = 0x040,    //!< Camera supports absolute zoom.
            LogicalPositionSpaceCapability      = 0x080,    //!< Camera supports absolute positioning in logical space ---
                                                            //! degrees for pan and tilt and width-based 35mm-equivalent focal length for zoom.

            AutomaticStateUpdateCapability      = 0x100,    //!< Camera updates its ptz-related state (e.g. flip & mirror) automatically, and
                                                            //! the user doesn't have to call \a updateState when it changes.
        };

        //!Returns bitset of \a Capability enumeration members
        virtual int getCapabilities() const = 0;

        //!Start continuous movement
        /*!
            All velocities are in range [-1..1]
            \return 0 on success, otherwise - error code
        */
        virtual int startMove( double panSpeed, double tiltSpeed, double zoomSpeed ) = 0;

        //!Stop continuous movement
        /*!
            \return 0 on success, otherwise - error code
        */
        virtual int stopMove() = 0;

        //!Move to absolute physical position
        /*!
            All values are in device-specific ranges. See \a getPhysicalPosition().
            \return 0 on success, otherwise - error code
        */
        virtual int setPhysicalPosition( double pan, double tilt, double zoom ) = 0;

        //!Get absolute physical position.
        /*!
            All returned values are in device-specific ranges and can be safely used only in
            subsequent calls to \a setPhysicalPosition().
            \return 0 on success, otherwise - error code
        */
        virtual int getPhysicalPosition( double* pan, double* tilt, double* zoom ) const = 0;

        //!Move to absolute logical position
        /*!
            Pan and tilt values are in degrees, zoom is in width-based 35mm-equivalent focal length. 
            This function must be implemented if \a LogicalPositionSpaceCapability is present.
            \return 0 on success, otherwise - error code
        */
        virtual int setLogicalPosition( double pan, double tilt, double zoom ) = 0;

        //!Get absolute logical position.
        /*!
            Pan and tilt values are in degrees, zoom is in 35mm equivalent.
            This function must be implemented if \a LogicalPositionSpaceCapability is present.
            \return 0 on success, otherwise - error code
        */
        virtual int getLogicalPosition( double* pan, double* tilt, double* zoom ) const = 0;

        struct LogicalPtzLimits {
            double minPan;
            double maxPan;
            double minTilt;
            double maxTilt;
            double minZoom;
            double maxZoom;
        };

        //!Get PTZ limits in logical space.
        /*!
            This function must be implemented if \a LogicalPositionSpaceCapability is present.
            \return 0 on success, otherwise - error code
        */
        virtual int getLogicalLimits(LogicalPtzLimits *limits) = 0;
        
        //!Updates stored camera state (e.g. flip & mirror) so that movement commands work properly.
        /*!
            This function is needed mainly for cameras that do not automatically
            adjust for flip, mirror and other changes in settings. 
            In case camera's state was changed, user can manually trigger camera 
            state update so that movement commands would work OK.

            This function must be implemented if \a AutomaticStateUpdateCapability is not present.
        */
        virtual int updateState() = 0;

        //!Returns text description of the last error
        /*!
            \param[out] errorString Buffer of size \a MAX_TEXT_LEN
        */
        virtual void getLastErrorString( char* errorString ) const = 0;
    };


    // {C6F06A48-8E3A-4690-8B21-CAC4A955D7ED}
    static const nxpl::NX_GUID IID_CameraMotionDataProvider = { { 0xc6, 0xf0, 0x6a, 0x48, 0x8e, 0x3a, 0x46, 0x90, 0x8b, 0x21, 0xca, 0xc4, 0xa9, 0x55, 0xd7, 0xed } };

    //!Provides access to motion detection support, implemented on camera
    class CameraMotionDataProvider
    :
        public nxpl::PluginInterface
    {
    public:
        //TODO for later use
    };


    class CameraInputEventHandler;

    // {872F473F-72CF-4397-81E6-C06D42E97113}
    static const nxpl::NX_GUID IID_CameraRelayIOManager = { { 0x87, 0x2f, 0x47, 0x3f, 0x72, 0xcf, 0x43, 0x97, 0x81, 0xe6, 0xc0, 0x6d, 0x42, 0xe9, 0x71, 0x13 } };

    static const int MAX_ID_LEN = 64;
    static const int MAX_RELAY_PORT_COUNT = 32;

    //!Relay input/output management
    /*!
        It is implementation defined within which thread event (\a nxcip::CameraInputEventHandler::inputPortStateChanged) will be delivered to
    */
    class CameraRelayIOManager
    :
        public nxpl::PluginInterface
    {
    public:
        //!Returns list of IDs of available relay output ports
        /*!
            \param[out] idList Array of size \a MAX_RELAY_PORT_COUNT of \a MAX_ID_LEN - length strings. All strings MUST be NULL-terminated
            \param[out] idNum Size of returned id list
            \return 0 on success, otherwise - error code
        */
        virtual int getRelayOutputList( char** idList, int* idNum ) const = 0;

        //!Returns list of IDs of available relay input ports
        /*!
            \param[out] idList Array of size \a MAX_RELAY_PORT_COUNT of \a MAX_ID_LEN - length strings. All strings MUST be NULL-terminated
            \param[out] idNum Size of returned id list
            \return 0 on success, otherwise - error code
        */
        virtual int getInputPortList( char** idList, int* idNum ) const = 0;

        //!Change state of relay output port
        /*!
            \param[in] outputID NULL-terminated ID of output port
            \param[in] activate If non-zero, port should be activated (closed circuit), otherwise - deactivated (opened circuit)
            \param[in] autoResetTimeoutMS If non-zero, port MUST return to deactivated state in \a autoResetTimeoutMS millis
            \return 0 on success, otherwise - error code
        */
        virtual int setRelayOutputState(
            const char* outputID,
            int activate,
            unsigned int autoResetTimeoutMS ) = 0;

        //!Starts relay input monitoring or increments internal counter, if already started
        /*!
            \return 0 on success (or input is already monitored), otherwise - error code
            \note Multiple \a startInputPortMonitoring() require multiple \a stopInputPortMonitoring() call.
                E.g., if input is already monitored, increases internal counter
        */
        virtual int startInputPortMonitoring() = 0;

        //!Stops input port monitoring if internal monitoring counter has reached zero
        /*!
            Implementation MUST guarantee:\n
                - no \a CameraInputEventHandler::inputPortStateChanged method MUST be called after this method have returned
                - if \a CameraInputEventHandler::inputPortStateChanged is currently running in different thread, 
                    this method MUST block until \a CameraInputEventHandler::inputPortStateChanged has returned
            \note Multiple \a startInputPortMonitoring() require multiple \a stopInputPortMonitoring() call.
        */
        virtual void stopInputPortMonitoring() = 0;

        //!Registers \a handler as event receiver
        /*!
            Usually, events are delivered (by \a handler->inputPortStateChanged call) to some plugin internal thread
            \note Does not call \a handler->addRef
        */
        virtual void registerEventHandler( CameraInputEventHandler* handler ) = 0;

        //!Removes \a handler from event receiver list
        /*!
            If \a handler is not registered, nothing is done
            Implementation MUST guarantee:\n
                - no \a handler->inputPortStateChanged method MUST be called after this method have returned
                - if \a handler->inputPortStateChanged is currently running in different thread, 
                    this method MUST block until \a handler->inputPortStateChanged has returned
            
            \note \a handler->releaseRef() is not called in this method
        */
        virtual void unregisterEventHandler( CameraInputEventHandler* handler ) = 0;

        //!Returns text description of last error
        /*!
            \param[out] errorString Buffer of size \a MAX_TEXT_LEN
        */
        virtual void getLastErrorString( char* errorString ) const = 0;
    };


    // {CC1E749F-2EC6-4b73-BEC9-8F2AE9B7FCCE}
    static const nxpl::NX_GUID IID_CameraInputEventHandler = { { 0xcc, 0x1e, 0x74, 0x9f, 0x2e, 0xc6, 0x4b, 0x73, 0xbe, 0xc9, 0x8f, 0x2a, 0xe9, 0xb7, 0xfc, 0xce } };

    //!Receives events on input port state change
    class CameraInputEventHandler
    :
        public nxpl::PluginInterface
    {
    public:
        virtual ~CameraInputEventHandler() {}

        //!Called by \a CameraRelayIOManager on input port event
        /*!
            This method MUST not block
            \param[in] source
            \param[in] inputPortID NULL-terminated port ID
            \param[in] newState non-zero - port activated (closed), zero - deactivated (opened)
            \param[in] timestamp \a timestamp of event in millis since epoch (1970-01-01 00:00), UTC
        */
        virtual void inputPortStateChanged(
            CameraRelayIOManager* source,
            const char* inputPortID,
            int newState,
            unsigned long int timestamp ) = 0;
    };
}

#endif  //NX_CAMERA_PLUGIN_H
