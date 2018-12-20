#ifndef ITE_ADVANCED_SETTINGS_H
#define ITE_ADVANCED_SETTINGS_H

#include "rx_device.h"
#include "tx_device.h"

namespace ite
{
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
}

#endif
