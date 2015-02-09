#ifndef ITE_CAMERA_MANAGER_H
#define ITE_CAMERA_MANAGER_H

#include <vector>
#include <set>
#include <memory>
#include <mutex>
#include <atomic>

#include <plugins/camera_plugin.h>

#include "ref_counter.h"
#include "rx_device.h"

namespace ite
{
    class MediaEncoder;
    class VideoPacket;
    class DeviceMapper;

    //!
    class CameraManager : public nxcip::BaseCameraManager3
    {
        DEF_REF_COUNTER

    public:
        CameraManager(const nxcip::CameraInfo& info, const DeviceMapper * devMapper);
        virtual ~CameraManager();

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

        VideoPacket * nextPacket(unsigned encoderNumber);

        void openStream(unsigned encNo);
        void closeStream(unsigned encNo);

        // for DiscoveryManager

        void updateCameraInfo(const nxcip::CameraInfo& info);
        bool stopIfNeedIt();

        // for MediaEncoder

        const char * url() const { return m_info.url; }

    private:
        mutable std::mutex m_mutex;

        const DeviceMapper * m_devMapper;
        unsigned short m_txID;
        std::vector<std::shared_ptr<MediaEncoder>> m_encoders;

        std::set<unsigned> m_openedStreams;
        std::atomic_bool m_waitStop;
        time_t m_stopTime;

        RxDevicePtr m_rxDevice;
        bool m_loading;

        mutable const char * m_errorStr;
        nxcip::CameraInfo m_info;

        bool captureAnyRxDevice();
        RxDevicePtr captureFreeRxDevice();
        void freeDevice();

        void initEncoders();
        void stopEncoders();

        void reloadMedia();
        void tryLoad();

        bool stopStreams(bool force = false);
        void stopTimer()
        {
            m_waitStop = false;
            m_stopTime = 0;
        }

        std::unique_ptr<VideoPacket> nextDevPacket(unsigned& streamNum);

        typedef enum
        {
            STATE_NO_CAMERA,        // no Tx
            STATE_NO_FREQUENCY,     // frequency not set
            STATE_NO_RECEIVER,      // no Rx for Tx
            STATE_DEVICE_READY,     // got Rx for Tx
            STATE_STREAM_LOADING,   // loading data streams
            STATE_STREAM_READY,     // got data streams, no readers
            STATE_STREAM_LOST,      //
            STATE_STREAM_READING    // got readers
        } State;

        State checkState() const;

        //

        void getParamStr_Channel(std::string& s) const;
        void getParamStr_Present(std::string& s) const;
        void getParamStr_Strength(std::string& s) const;
        void getParamStr_Quality(std::string& s) const;

        void getParamStr_RxID(std::string& s) const;
        void getParamStr_RxCompany(std::string& s) const { if (m_rxDevice) s = m_rxDevice->rxDriverInfo().company; }
        void getParamStr_RxModel(std::string& s) const { if (m_rxDevice) s = m_rxDevice->rxDriverInfo().supportHWInfo; }
        void getParamStr_RxDriverVer(std::string& s) const { if (m_rxDevice) s = m_rxDevice->rxDriverInfo().driverVersion; }
        void getParamStr_RxAPIVer(std::string& s) const { if (m_rxDevice) s = m_rxDevice->rxDriverInfo().APIVersion; }
        void getParamStr_RxFwVer(std::string& s) const
        {
            if (m_rxDevice)
            {
                s = m_rxDevice->rxDriverInfo().fwVersionLink;
                s += "-";
                s += m_rxDevice->rxDriverInfo().fwVersionOFDM;
            }
        }

        void getParamStr_TxHwID(std::string& s) const { if (m_rxDevice) s = m_rxDevice->txDriverInfo().hardwareId; }
        void getParamStr_TxCompany(std::string& s) const { if (m_rxDevice) s = m_rxDevice->txDriverInfo().companyName; }
        void getParamStr_TxModel(std::string& s) const { if (m_rxDevice) s = m_rxDevice->txDriverInfo().modelName; }
        void getParamStr_TxSerial(std::string& s) const { if (m_rxDevice) s = m_rxDevice->txDriverInfo().serialNumber; }
        void getParamStr_TxFwVer(std::string& s) const { if (m_rxDevice) s = m_rxDevice->txDriverInfo().firmwareVersion; }

        bool setParam_Channel(std::string& s);
    };
}

#endif // ITE_CAMERA_MANAGER_H
