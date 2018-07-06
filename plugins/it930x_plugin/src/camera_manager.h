#ifndef ITE_CAMERA_MANAGER_H
#define ITE_CAMERA_MANAGER_H

#include <vector>
#include <set>
#include <map>
#include <memory>
#include <mutex>

#include <camera/camera_plugin.h>

#include "ref_counter.h"
#include "rx_device.h"
#include "tx_device.h"
#include "timer.h"

namespace ite
{
    class MediaEncoder;
    class DeviceMapper;
    class DevReader;
    class CameraManager;

    ///
    class CameraManager : public DefaultRefCounter<nxcip::BaseCameraManager3>
    {
    public:
        CameraManager(const RxDevicePtr &rxDev);

        // nxcip::PluginInterface

        virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;

        // nxcip::BaseCameraManager

        virtual int getEncoderCount( int* encoderCount ) const override;
        virtual int getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr ) override;
        virtual int getCameraInfo( nxcip::CameraInfo* info ) const override;
        virtual int getCameraCapabilities( unsigned int* capabilitiesMask ) const override;
        virtual void setCredentials( const char* username, const char* password ) override;
        virtual int setAudioEnabled( int audioEnabled ) override;
        virtual nxcip::CameraPtzManager* getPtzManager() const override;
        virtual nxcip::CameraMotionDataProvider* getCameraMotionDataProvider() const override;
        virtual nxcip::CameraRelayIOManager* getCameraRelayIOManager() const override;
        virtual void getLastErrorString( char* errorString ) const override;

        // nxcip::BaseCameraManager2

        virtual int createDtsArchiveReader( nxcip::DtsArchiveReader** dtsArchiveReader ) const override;
        virtual int find( nxcip::ArchiveSearchOptions* searchOptions, nxcip::TimePeriods** timePeriods ) const override;
        virtual int setMotionMask( nxcip::Picture* motionMask ) override;

        // nxcip::BaseCameraManager3

        virtual const char * getParametersDescriptionXML() const override;
        virtual int getParamValue(const char * paramName, char * valueBuf, int * valueBufSize) const override;
        virtual int setParamValue(const char * paramName, const char * value) override;

        // for StreamReader

        std::weak_ptr<RxDevice> rxDevice() const { return m_rxDevice; }

        void openStream(unsigned encNo);
        void closeStream(unsigned encNo);

        void updateCameraInfo(const nxcip::CameraInfo& info);
        bool stopIfNeedIt();

        // for MediaEncoder

        const char * url() const { return m_info.url; }

       //

        void needUpdate(unsigned group);
        void updateSettings();
        void setChannel(unsigned channel) { m_newChannel = channel; }

        typedef enum
        {
            STATE_NO_CAMERA,        // no Tx
            STATE_NO_RECEIVER,      // no Rx for Tx
            STATE_NOT_LOCKED,       // got Rx for Tx
            STATE_NO_ENCODERS,      //
            STATE_READY,            // got data streams, no readers
            STATE_READING           // got readers
        } State;

        State tryLoad();

        //refactor
        int cameraId() const
        {
            return m_cameraId;
        }

        std::mutex &get_mutex() {return m_mutex;}
        RxDevicePtr &rxDeviceRef() { return m_rxDevice; }

    private:
        typedef MediaEncoder *MediaEncoderPtr;
        mutable std::mutex m_mutex;

        RxDevicePtr m_rxDevice;

        std::set<unsigned> m_openedStreams;
        Timer m_stopTimer;

        std::map<uint8_t, bool> m_update;
        unsigned m_newChannel;

        mutable const char * m_errorStr;
        mutable nxcip::CameraInfo m_info;

        bool stopStreams(bool force = false);

        const int m_cameraId;

        State checkState() const;
    };
}

#endif // ITE_CAMERA_MANAGER_H
