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
        CameraManager(const nxcip::CameraInfo& info, DeviceMapper * devMapper, TxDevicePtr txDev);
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
        std::weak_ptr<RxDevice> rxDevice() const { return m_rxDevice; }
        void processRC();

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

        DeviceMapper * m_devMapper;
        TxDevicePtr m_txDev;
        RxDevicePtr m_rxDevice;
        std::vector<std::shared_ptr<MediaEncoder>> m_encoders; // FIXME: do not use smart ptr for ref-counted object

        std::set<unsigned> m_openedStreams;
        Timer m_stopTimer;

        mutable const char * m_errorStr;
        nxcip::CameraInfo m_info;

        bool captureAnyRxDevice();
        RxDevicePtr captureFreeRxDevice();
        void freeDevice();

        void initEncoders();
        void stopEncoders();

        bool configureTx();
        void reloadMedia();

        bool stopStreams(bool force = false);

        typedef enum
        {
            STATE_NO_CAMERA,        // no Tx
            STATE_NO_RECEIVER,      // no Rx for Tx
            STATE_NOT_LOCKED,       // got Rx for Tx
            STATE_NOT_CONFIGURED,   // reading Tx configuration
            STATE_NO_ENCODERS,      //
            STATE_READY,            // got data streams, no readers
            STATE_READING           // got readers
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

        void getParamStr_TxHwID(std::string& s) const { if (m_rxDevice) s = TxManufactureInfo(*m_rxDevice->txDevice()).hardwareId; }
        void getParamStr_TxCompany(std::string& s) const { if (m_rxDevice) s = TxManufactureInfo(*m_rxDevice->txDevice()).companyName; }
        void getParamStr_TxModel(std::string& s) const { if (m_rxDevice) s = TxManufactureInfo(*m_rxDevice->txDevice()).modelName; }
        void getParamStr_TxSerial(std::string& s) const { if (m_rxDevice) s = TxManufactureInfo(*m_rxDevice->txDevice()).serialNumber; }
        void getParamStr_TxFwVer(std::string& s) const { if (m_rxDevice) s = TxManufactureInfo(*m_rxDevice->txDevice()).firmwareVersion; }

        bool setParam_Channel(std::string& s);
    };
}

#endif // ITE_CAMERA_MANAGER_H
