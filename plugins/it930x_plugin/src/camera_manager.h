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
#include "tx_device.h"
#include "timer.h"

namespace ite
{
    class MediaEncoder;
    class DeviceMapper;
    class DevReader;

    //!
    class CameraManager : public nxcip::BaseCameraManager3, public ObjectCounter<CameraManager>
    {
        DEF_REF_COUNTER

    public:
        CameraManager(const nxcip::CameraInfo& info, const DeviceMapper * devMapper, TxDevicePtr txDev);
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

        DevReader * devReader() const;
        void minorWork();

        void openStream(unsigned encNo);
        void closeStream(unsigned encNo);

        // for DiscoveryManager

        void updateTx(TxDevicePtr txDev)
        {
            if (! m_txDev)
                m_txDev = txDev;
        }

        void updateCameraInfo(const nxcip::CameraInfo& info);
        bool stopIfNeedIt();

        // for MediaEncoder

        const char * url() const { return m_info.url; }

        unsigned short txID() const
        {
            if (m_txDev)
                return m_txDev->txID();
            return 0;
        }

    private:
        mutable std::mutex m_mutex;

        const DeviceMapper * m_devMapper;
        TxDevicePtr m_txDev;
        std::vector<std::shared_ptr<MediaEncoder>> m_encoders; // FIXME: do not use smart ptr for ref-counted object

        std::set<unsigned> m_openedStreams;
        Timer m_stopTimer;

        RxDevicePtr m_rxDevice;

        mutable const char * m_errorStr;
        nxcip::CameraInfo m_info;

        bool captureAnyRxDevice();
        RxDevicePtr captureFreeRxDevice();
        void freeDevice();

        void initEncoders();
        void stopEncoders();

        void reloadMedia();

        bool stopStreams(bool force = false);

        typedef enum
        {
            STATE_NO_CAMERA,        // no Tx
            STATE_NO_CHANNEL,       // channel not set
            STATE_NO_RECEIVER,      // no Rx for Tx
            STATE_DEVICE_READY,     // got Rx for Tx
            STATE_STREAM_LOADING,   // loading data streams
            STATE_STREAM_READY,     // got data streams, no readers
            STATE_STREAM_LOST,      //
            STATE_STREAM_READING    // got readers
        } State;

        State checkState() const;
        State tryLoad();

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

        void getParamStr_TxHwID(std::string& s) const { if (m_rxDevice) s = m_rxDevice->txDevice()->txDeviceInfo().hardwareId; }
        void getParamStr_TxCompany(std::string& s) const { if (m_rxDevice) s = m_rxDevice->txDevice()->txDeviceInfo().companyName; }
        void getParamStr_TxModel(std::string& s) const { if (m_rxDevice) s = m_rxDevice->txDevice()->txDeviceInfo().modelName; }
        void getParamStr_TxSerial(std::string& s) const { if (m_rxDevice) s = m_rxDevice->txDevice()->txDeviceInfo().serialNumber; }
        void getParamStr_TxFwVer(std::string& s) const { if (m_rxDevice) s = m_rxDevice->txDevice()->txDeviceInfo().firmwareVersion; }

        bool setParam_Channel(std::string& s);
    };
}

#endif // ITE_CAMERA_MANAGER_H
