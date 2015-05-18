#ifndef ITE_CAMERA_MANAGER_H
#define ITE_CAMERA_MANAGER_H

#include <vector>
#include <set>
#include <map>
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
    class CameraManager;

    ///
    class AdvancedSettings
    {
    public:
        enum
        {
            GROUP_SIGNAL,
            GROUP_RX,
            GROUP_TX,
            GROUP_OSD
        };

        AdvancedSettings(TxDevicePtr txDev, RxDevicePtr rxDev)
        :   m_txDev(txDev), m_rxDev(rxDev)
        {}

        static const char * getDescriptionXML();
        int getParamValue(const char * paramName, char * valueBuf, int * valueBufSize) const;
        int setParamValue(CameraManager& camera, const char * paramName, const char * value);

    private:
        void getChannel(std::string& s) const;
        void getPresent(std::string& s) const       { s = m_rxDev->present() ? "true" : "false"; }
        void getStrength(std::string& s) const      { s = num2str(m_rxDev->strength()); }
        void getQuality(std::string& s) const       { s = num2str(m_rxDev->quality()); }

        void getRxID(std::string& s) const          { s = num2str(m_rxDev->rxID()); }
        void getRxCompany(std::string& s) const     { s = m_rxDev->rxDriverInfo().company; }
        void getRxModel(std::string& s) const       { s = m_rxDev->rxDriverInfo().supportHWInfo; }
        void getRxDriverVer(std::string& s) const   { s = m_rxDev->rxDriverInfo().driverVersion; }
        void getRxAPIVer(std::string& s) const      { s = m_rxDev->rxDriverInfo().APIVersion; }
        void getRxFwVer(std::string& s) const
        {
            s = m_rxDev->rxDriverInfo().fwVersionLink;
            s += "-";
            s += m_rxDev->rxDriverInfo().fwVersionOFDM;
        }

        void getTxID(std::string& s) const          { s = num2str(m_txDev->txID()); }
        void getTxHwID(std::string& s) const        { s = TxManufactureInfo(*m_txDev).hardwareId(); }
        void getTxCompany(std::string& s) const     { s = TxManufactureInfo(*m_txDev).companyName(); }
        void getTxModel(std::string& s) const       { s = TxManufactureInfo(*m_txDev).modelName(); }
        void getTxSerial(std::string& s) const      { s = TxManufactureInfo(*m_txDev).serialNumber(); }
        void getTxFwVer(std::string& s) const       { s = TxManufactureInfo(*m_txDev).firmwareVersion(); }

        void getFormat(const char * value, uint8_t& format) const;
        void getPosition(const char * value, uint8_t& enabled, uint8_t& position) const;
        unsigned getPositionVariant(uint8_t enabled, uint8_t position) const;

        void getOsdDatePosition(std::string& s) const;
        void getOsdDateFormat(std::string& s) const;
        void getOsdTimePosition(std::string& s) const;
        void getOsdTimeFormat(std::string& s) const;
        void getOsdLogoPosition(std::string& s) const;
        void getOsdLogoFormat(std::string& s) const;
        void getOsdInfoPosition(std::string& s) const;
        void getOsdInfoFormat(std::string& s) const;
        void getOsdTextPosition(std::string& s) const;
        void getOsdText(std::string& s) const;

        TxDevicePtr m_txDev;
        RxDevicePtr m_rxDev;
    };

    ///
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
        void processRC(bool update = false);

        void openStream(unsigned encNo);
        void closeStream(unsigned encNo);

        // for DiscoveryManager

        void updateTx(TxDevicePtr txDev)
        {
            if (! m_txDevice)
                m_txDevice = txDev;
        }

        void updateCameraInfo(const nxcip::CameraInfo& info);
        bool stopIfNeedIt();

        // for MediaEncoder

        const char * url() const { return m_info.url; }

        unsigned short txID() const
        {
            if (m_txDevice)
                return m_txDevice->txID();
            return 0;
        }

        //

        void needUpdate(unsigned group);
        void updateSettings();
        void setChannel(unsigned channel) { m_newChannel = channel; }

    private:
        mutable std::mutex m_mutex;

        DeviceMapper * m_devMapper;
        TxDevicePtr m_txDevice;
        RxDevicePtr m_rxDevice;
        std::vector<std::shared_ptr<MediaEncoder>> m_encoders; // FIXME: do not use smart ptr for ref-counted object

        std::set<unsigned> m_openedStreams;
        Timer m_stopTimer;

        std::map<uint8_t, bool> m_update;
        unsigned m_newChannel;

        mutable const char * m_errorStr;
        nxcip::CameraInfo m_info;

        bool captureAnyRxDevice();
        RxDevicePtr captureFreeRxDevice();
        void freeRx(bool reset = false);

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
    };
}

#endif // ITE_CAMERA_MANAGER_H
