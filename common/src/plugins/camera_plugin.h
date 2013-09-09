#ifndef NX_CAMERA_PLUGIN_H
#define NX_CAMERA_PLUGIN_H

#include "camera_plugin_types.h"
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
    typedef unsigned long long int UsecUTCTimestamp;
    static const UsecUTCTimestamp INVALID_TIMESTAMP_VALUE = (UsecUTCTimestamp)-1LL;

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
    static const int NX_NO_DATA = -24;
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


    class StreamReader;

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


        // {9A1BDA18-563C-42de-8E23-B9244FD00658}
    static const nxpl::NX_GUID IID_CameraMediaEncoder2 = { { 0x9a, 0x1b, 0xda, 0x18, 0x56, 0x3c, 0x42, 0xde, 0x8e, 0x23, 0xb9, 0x24, 0x4f, 0xd0, 0x6, 0x58 } };

    //!Extends \a CameraMediaEncoder by adding funtionality for plugin to directly provide live media stream
    class CameraMediaEncoder2
    :
        public CameraMediaEncoder
    {
    public:
        virtual ~CameraMediaEncoder2() {}

        //!Returns stream reader, providing live data stream
        /*!
            This method is only used if \a BaseCameraManager::nativeMediaStreamCapability is present, otherwise \a CameraMediaEncoder::getMediaUrl is used

            Can be used if camera uses some proprietary media stream control protocol or wants to provide motion information
        */
        virtual StreamReader* getLiveStreamReader() = 0;
    };


    class CameraPTZManager;
    class CameraMotionDataProvider;
    class CameraRelayIOManager;
    class DtsArchiveReader;

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

            \param[in] encoderIndex encoder index starts with 0
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
            hardwareMotionCapability    = 0x0001,     //!< camera supports hardware motion. Plugin, returning this flag, MUST implement \a BaseCameraManager::nativeMediaStreamCapability also
            relayInputCapability        = 0x0002,     //!< if this flag is enabled, \a CameraRelayIOManager MUST be implemented
            relayOutputCapability       = 0x0004,     //!< if this flag is enabled, \a CameraRelayIOManager MUST be implemented
            ptzCapability               = 0x0008,     //!< if this flag is enabled, \a CameraPTZManager MUST be implemented
            audioCapability             = 0x0010,     //!< if set, camera supports audio
            shareFpsCapability          = 0x0020,     //!< if second stream is running whatever fps it has => first stream can get maximumFps - secondstreamFps
            sharePixelsCapability       = 0x0040,     //!< if second stream is running whatever megapixel it has => first stream can get maxMegapixels - secondstreamPixels
            shareIpCapability           = 0x0080,     //!< allow multiple instances on a same IP address
            dtsArchiveCapability        = 0x0100,    //!< camera has archive storage and provides access to its archive
            nativeMediaStreamCapability = 0x0200     //!< provides media stream through \a StreamReader interface, otherwise - \a CameraMediaEncoder::getMediaUrl is used
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
            \deprecated To receive motion data, use 
        */
        virtual CameraMotionDataProvider* getCameraMotionDataProvider() const = 0;
        //!MUST return not-NULL if \a BaseCameraManager::relayInputCapability is present
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


    // {1181F23B-071C-4608-89E3-648E1A735B54}
    static const nxpl::NX_GUID IID_BaseCameraManager2 = { { 0x11, 0x81, 0xf2, 0x3b, 0x07, 0x1c, 0x46, 0x08, 0x89, 0xe3, 0x64, 0x8e, 0x1a, 0x73, 0x5b, 0x54 } };

    //!Extends \a BaseCameraManager by adding remote archive storage support (e.g., storage is mounted directly to camera)
    class BaseCameraManager2
    :
        public BaseCameraManager
    {
    public:
        virtual ~BaseCameraManager2() {}

        //!Returns not NULL if \a BaseCameraManager::dtsArchiveCapability is supported
        /*!
            Always creates new object (to allow simultaneous multiple connections to archive)
            \param[out] dtsArchiveReader Used to return archive reader on success
            \return \b NX_NO_ERROR on success, otherwise - error code (in this case \a *dtsArchiveReader set to NULL)
        */
        virtual int createDtsArchiveReader( DtsArchiveReader** dtsArchiveReader ) const = 0;
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

        //!Get absolute logical position
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

        //!Get PTZ limits in logical space
        /*!
            This function must be implemented if \a LogicalPositionSpaceCapability is present.
            \return 0 on success, otherwise - error code
        */
        virtual int getLogicalLimits(LogicalPtzLimits *limits) = 0;
        
        //!Updates stored camera state (e.g. flip & mirror) so that movement commands work properly
        /*!
            This function is needed mainly for cameras that do not automatically
            adjust for flip, mirror and other changes in settings. 
            In case camera's state was changed, user can manually trigger camera 
            state update so that movement commands would work OK.

            This function must be implemented if \a AutomaticStateUpdateCapability is not present.
        */
        virtual int updateState() = 0;
    };


    //!Type of packets provided by \a StreamReader
    enum DataPacketType
    {
        dptAudio,
        dptVideo
    };


    // {763C93DC-A77D-41ff-8071-B64C4D3AFCFF}
    static const nxpl::NX_GUID IID_MediaDataPacket = { { 0x76, 0x3c, 0x93, 0xdc, 0xa7, 0x7d, 0x41, 0xff, 0x80, 0x71, 0xb6, 0x4c, 0x4d, 0x3a, 0xfc, 0xff } };

    //!Portion of media data
    class MediaDataPacket
    :
        public nxpl::PluginInterface
    {
    public:
        enum Flags
        {
            //!e.g., h.264 IDR frame
            fKeyPacket          = 0x01,
            //!packet has been generated during playback of reverse stream
            fReverseStream      = 0x02,
            //!set in first packet of gop block of reverse stream (see \a nxcip::DtsArchiveReader::setReverseMode)
            fReverseBlockStart  = 0x04,
            //!packet belongs to low-quality stream. If unset, assuming frame is in high quality
            fLowQuality         = 0x08,
            /*!
                MUST be set after each \a nxcip::DtsArchiveReader::seek, \a nxcip::DtsArchiveReader::reverseModeCapability, 
                \a nxcip::DtsArchiveReader::setQuality to signal discontinuity in timestamp
            */
            fStreamReset        = 0x10
        };

        //!Packet's timestamp (usec since 1970-01-01, UTC)
        virtual UsecUTCTimestamp timestamp() const = 0;
        //!Packet type
        virtual DataPacketType type() const = 0;
        //!Coded media stream data
        /*!
            Data format for different codecs:\n
                - h.264 (\a nxcip::CODEC_ID_H264): [iso-14496-10, AnnexB] byte stream. SPS and PPS MUST be available in the stream. 
                    It is recommended that SPS and PPS are repeated before each group of pictures
                - motion jpeg (\a nxcip::CODEC_ID_MJPEG): Each packet is a complete jpeg picture
                - aac (\a nxcip::CODEC_ID_AAC): ADTS stream
            \return Media data. Returned buffer MUST be aligned on 32-byte boundary (this restriction helps for some optimization)
        */
        virtual const void* data() const = 0;
        //!Returns size (in bytes) of packet's data
        virtual unsigned int dataSize() const = 0;
        /*!
            For video packet data contains number of camera sensor starting with 0 (e.g., panoramic camera has multiple sensors).\n
            For audio, this is audio track number (in case of multiple microphones on camera device)
        */
        virtual unsigned int channelNumber() const = 0;
        //!Constant from \a nxcip::CompressionType enumeration
        virtual CompressionType codecType() const = 0;
        //!Returns combination of values from \a MediaDataPacket::Flags enumeration
        virtual unsigned int flags() const = 0;
    };


    // {C693330F-B176-4915-A784-816A643F63F9}
    static const nxpl::NX_GUID IID_MotionData = { { 0xc6, 0x93, 0x33, 0x0f, 0xb1, 0x76, 0x49, 0x15, 0xa7, 0x84, 0x81, 0x6a, 0x64, 0x3f, 0x63, 0xf9 } };

    //!Contains information about motion in different parts of video picture
    class MotionData
    :
        public nxpl::PluginInterface
    {
    public:
        //TODO/DECL
    };


    // {A85D884B-F05E-4fff-8B5A-E36570E73067}
    static const nxpl::NX_GUID IID_VideoDataPacket = { { 0xa8, 0x5d, 0x88, 0x4b, 0xf0, 0x5e, 0x4f, 0xff, 0x8b, 0x5a, 0xe3, 0x65, 0x70, 0xe7, 0x30, 0x67 } };

    //!Video packet. MUST contain complete frame (or field in case of interlaced video)
    class VideoDataPacket
    :
        public MediaDataPacket
    {
    public:
        //!Returns motion data. Can be NULL
        /*!
            Can return same object with each call, incrementing ref count
        */
        virtual MotionData* getMotionData() const = 0;
    };


    // {AFE4EEDA-3770-42c3-8381-EE3B55522551}
    static const nxpl::NX_GUID IID_StreamReader = { { 0xaf, 0xe4, 0xee, 0xda, 0x37, 0x70, 0x42, 0xc3, 0x83, 0x81, 0xee, 0x3b, 0x55, 0x52, 0x25, 0x51 } };

    //!Used for reading media stream from camera
    /*!
        Provides synchronous API to receiving media stream
        \note returns stream of media data of different types (video, audio, motion)
        \note This class itself does not add any delay into media stream. Data is returned upon its availability. 
            E.g., while reading media stream from file with 30fps, actual data rate is not limited with 30fps, but with read speed only
    */
    class StreamReader
    :
        public nxpl::PluginInterface
    {
    public:
        //!Returns media packet or NULL in case of error
        /*!
            If no data is available, blocks till some data becomes available or \a StreamReader::interrupt had been called
            \param packet
            \return error code (\a nxcip::NX_NO_ERROR on success)
            \note Returns packet has its ref counter set to 1
            \note On end of data, \a nxcip::NX_NO_ERROR is returned and \a packet is set to NULL
        */
        virtual int getNextData( MediaDataPacket** packet ) = 0;

        //!Interrupt \a StreamReader::getNextData blocked in other thread
        virtual void interrupt() = 0;
    };


    // {B0F07EAF-A59E-41fc-8F9E-86C323218554}
    static const nxpl::NX_GUID IID_Iteratable = { { 0xb0, 0xf0, 0x7e, 0xaf, 0xa5, 0x9e, 0x41, 0xfc, 0x8f, 0x9e, 0x86, 0xc3, 0x23, 0x21, 0x85, 0x54 } };

    //!Interface for class-container with element iterating support
    /*!
        \code{.cpp}
        Iteratable* it;
        for( it->goToBeginning(); it->next(); )
        {
            //processing data ...
        }
        \endcode
    */
    class Iteratable
    :
        public nxpl::PluginInterface
    {
    public:
        //!Move cursor to the beginning (following \a Iteratable::next() call will move cursor to the first position of dataset)
        virtual void goToBeginning() = 0;
        /*!
            \return true, if cursor is set to the next position. false, if already at the end of data
        */
        virtual bool next() = 0;
    };


    // {8006CC9F-7BDD-4a4c-8920-AC5546D4924A}
    static const nxpl::NX_GUID IID_TimePeriods = { { 0x80, 0x06, 0xcc, 0x9f, 0x7b, 0xdd, 0x4a, 0x4c, 0x89, 0x20, 0xac, 0x55, 0x46, 0xd4, 0x92, 0x4a } };

    //!Array of time periods
    class TimePeriods
    :
        public Iteratable
    {
    public:
        /*!
            \param[out] start Start of time period (usec since 1970-01-01, UTC)
            \param[out] end End of time period (usec since 1970-01-01, UTC)
            \return \a true, if data present (cursor is on valid position)
        */
        virtual bool get( UsecUTCTimestamp* start, UsecUTCTimestamp* end ) = 0;
    };


    enum MediaStreamQuality
    {
        //!Default quality SHOULD be high quality
        msqDefault = 0,
        msqHigh,
        msqLow
    };


    // {CEB97832-E931-4965-9B18-A8A1557107D7}
    static const nxpl::NX_GUID IID_DtsArchiveReader = { { 0xce, 0xb9, 0x78, 0x32, 0xe9, 0x31, 0x49, 0x65, 0x9b, 0x18, 0xa8, 0xa1, 0x55, 0x71, 0x7, 0xd7 } };

    //!Provides access to archive, stored on camera
    /*!
        Does not provide media stream itself, but provides methods to manage (seek, select playback direction, select stream source (encoder) ) stream, 
            provided by \a StreamReader instance

        It is not required, that archive contains data from all encoders
        \note By default, archive is positioned to the beginning, quality set to high and playback direction is forward
    */
    class DtsArchiveReader
    :
        public nxpl::PluginInterface
    {
    public:
        enum Capabilities
        {
            //!Signals that reverse playback by GOP (group of picture) reordering is supported
            /*!
                Plugin MAY support backward playback by GOP (group of picture) reordering. If this is not supported, reverse playback is emulated by media server.\n
                This capability is not required but can increase performance.\n

                Reverse gop playback is organized in following way:\n
                Assume, we have media stream consisting of 6 gop:\n

                \code
                ------ ------ ------ ------ ------ ------
                |GOP1| |GOP2| |GOP3| |GOP4| |GOP5| |GOP6|
                ------ ------ ------ ------ ------ ------
                \endcode

                To be able to play it in reverse mode, plugin passes GOPs in reverse mode (RB is "reverse block"):\n

                \code
                ------ | ------ | ------ | ------ | ------ | ------
                |GOP6| | |GOP5| | |GOP4| | |GOP3| | |GOP2| | |GOP1|
                ------ | ------ | ------ | ------ | ------ | ------
                  RB1  |   RB2  |   RB3  |   RB4  |   RB5  |   RB6
                \endcode

                Reverse block can consist of multiple GOP. E.g.:\n

                \code
                ------ ------ | ------ ------ | ------ ------
                |GOP5| |GOP6| | |GOP3| |GOP4| | |GOP1| |GOP2|
                ------ ------ | ------ ------ | ------ ------
                     RB1      |      RB2      |      RB3
                \endcode

                - each packet in this stream MUST contain \a MediaDataPacket::fReverseStream flag
                - first packet of each GOP MUST be key-frame and contain \a MediaDataPacket::fKeyPacket flag
                - first packet of each RB MUST contain \a MediaDataPacket::fReverseBlockStart flag

                Such stream is played by decoding the whole RB and then playing decoded frames in reverse order

                \note Frames within each GOP MUST be in their original order (i.e., decoding order)
                \note Once again, this is an optional functionality for plugin
            */
            reverseGopModeCapability        = 0x01,
            //!frame skipping with \a DtsArchiveReader::setSkipFrames method is supported
            /*!
                \note This funtionality is optional. Helps for better performance
            */
            skipFramesCapability            = 0x02,
            //!motion data can be provided in media stream
            motionDataCapability            = 0x04,
            //!if present, \a nxcip::DtsArchiveReader::find is implemented
            searchByMotionMaskCapability    = 0x08
        };

        //!Returns bit mask with supported capabilities
        virtual unsigned int getCapabilities() const = 0;

        //!Initialize connection to archive
        /*!
            \return \a false, if failed. Use \a DtsArchiveReader::getLastErrorString to get error description
        */
        virtual bool open() = 0;
        //!Returns stream reader
        /*!
            \a DtsArchiveReader instance holds reference to returned \a StreamReader instance
        */
        virtual StreamReader* getStreamReader() = 0;
        //!Returns timestamp (usec since 1970-01-01, UTC) of oldest data, present in the archive
        /*!
            This value can be changed at any time (if record is ongoing)
        */
        virtual UsecUTCTimestamp startTime() const = 0;
        //!Returns timestamp (usec since 1970-01-01, UTC) of newest data, present in the archive
        /*!
            This value can be changed at any time (if record is ongoing)
        */
        virtual UsecUTCTimestamp endTime() const = 0;
        //!Seek to specified posiition in stream
        /*!
            Implementation is allowed to jump to point preceding requested \a timestamp
            \param[in] timestamp timestamp to seek to
            \param[in] findKeyFrame If \a true, MUST jump to key-frame only (selected frame timestamp MUST be equal or less than requested)
            \param[out] selectedPosition Timestamp of actually selected position
            \return \a NX_NO_ERROR on success, otherwise - error code
            \note This funtionality is required
        */
        virtual int seek( UsecUTCTimestamp timestamp, bool findKeyFrame, UsecUTCTimestamp* selectedPosition ) = 0;
        //!Set playback direction (forward/reverse)
        /*!
            If \a timestamp is not equal to \a INVALID_TIMESTAMP_VALUE, seek is performed along with playback direction change
            \param[in] isReverse If true, playback 
            \param[in] position if not \a INVALID_TIMESTAMP_VALUE, playback SHOULD jump to this position (with rules, defined for \a DtsArchiveReader::seek)
            \note This method is used only if \a DtsArchiveReader::reverseModeCapability is supported
            \return \a NX_NO_ERROR on success, otherwise - error code
            \note This funtionality is optional
        */
        virtual int setReverseMode( bool isReverse, UsecUTCTimestamp timestamp ) = 0;
        //!Toggle motion data in media stream on/off
        /*!
            \return \a NX_NO_ERROR on success, otherwise - error code
            \note This funtionality is optional
        */
        virtual int setMotionDataEnabled( bool motionPresent ) = 0;
        //!Select media stream quality (used for dynamic media stream adaptation)
        /*!
            Media stream switching may not occur immediately, depending on \a waitForKeyFrame argument
            \param[in] quality Media stream quality
            \param[in] waitForKeyFrame If \a true, implementation SHOULD switch only on reaching next key frame of new stream.
                If \a false, \a StreamReader instance SHOULD switch immediately to the key frame of new stream with greatest pts, not exceeding current
            \return \a NX_NO_ERROR on success (requested data present in the stream). Otherwise - error code.\n
                If requested quality is not supported, \a NX_NO_DATA is returned
            \note \a nxcip::msqDefault MUST always be supported
            \note This funtionality is optional
        */
        virtual int setQuality( MediaStreamQuality quality, bool waitForKeyFrame ) = 0;
        //!Enable/disable skipping frames
        /*!
            Tells to skip media packets for inter-packet timestamp gap to be at least \a step usec.
            When frame skipping is implied, audio packets SHOULD not be reported
            \param step If 0, no frames MUST be skipped
            \note Used only if \a DtsArchiveReader::skipFramesCapability is set
            \note If frame skipping is enabled only key frames SHOULD be provided by \a StreamReader::getNextData call
            \note This is used to generate thumbnails
            \note This funtionality is optional
        */
        virtual int setSkipFrames( UsecUTCTimestamp step ) = 0;
        //!Find periods of archive, where motion occured on specified region of video
        /*!
            This method is only used when \a DtsArchiveReader::searchByMotionMaskCapability is present
            \param[in] motionMask
            \param[out] timePeriods
            \return \a NX_NO_ERROR on success (requested data present in the stream). Otherwise - error code
            \note If nothing found, \a NX_NO_ERROR is returned and \a timePeriods is set to \a NULL
        */
        virtual int find( MotionData* motionMask, TimePeriods** timePeriods ) = 0;

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
        //TODO for future use
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
