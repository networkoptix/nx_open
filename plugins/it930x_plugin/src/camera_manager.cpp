#include <string>
#include <chrono>
#include <atomic>
#include <ctime>

#include "camera_manager.h"
#include "media_encoder.h"

#include "discovery_manager.h"

namespace
{
    class AdvParam
    {
    public:
        AdvParam(const char * name)
        :   m_name(name)
        {}

        bool operator == (const char * name)
        {
            if (!name)
                return false;
            return strcmp(m_name, name) == 0;
        }

        const char * name() const { return m_name; }

    private:
        const char * m_name;
    };

    AdvParam PARAM_ALL_OPEN = "<all>";
    AdvParam PARAM_ALL_CLOSE = "</all>";

    AdvParam PARAM_SIGNAL_CHANNEL =         "channel";
    AdvParam PARAM_SIGNAL_PRESENT =         "present";
    AdvParam PARAM_SIGNAL_STRENGTH =        "strength";
    AdvParam PARAM_SIGNAL_QUALITY =         "quality";

    AdvParam PARAM_RX_ID =                  "rxID";
    AdvParam PARAM_RX_COMPANY =             "rxCompany";
    AdvParam PARAM_RX_MODEL =               "rxModel";
    AdvParam PARAM_RX_DRIVER_VER =          "rxDriverVer";
    AdvParam PARAM_RX_API_VER =             "rxAPIVer";
    AdvParam PARAM_RX_FW_VER =              "rxFWVer";

    AdvParam PARAM_TX_ID =                  "txID";
    AdvParam PARAM_TX_HWID =                "txHwID";
    AdvParam PARAM_TX_COMPANY =             "txCompany";
    AdvParam PARAM_TX_MODEL =               "txModel";
    AdvParam PARAM_TX_SERIAL =              "txSerial";
    AdvParam PARAM_TX_FW_VER =              "txFWVer";

    AdvParam RAPAM_OSD_DATE_POSITION =      "osdDatePos";
    AdvParam RAPAM_OSD_DATE_FORMAT =        "osdDateFmt";
    AdvParam RAPAM_OSD_TIME_POSITION =      "osdTimePos";
    AdvParam RAPAM_OSD_TIME_FORMAT =        "osdTimeFmt";
    AdvParam RAPAM_OSD_LOGO_POSITION =      "osdLogoPos";
    AdvParam RAPAM_OSD_LOGO_FORMAT =        "osdLogoFmt";
    AdvParam RAPAM_OSD_INFO_POSITION =      "osdInfoPos";
    AdvParam RAPAM_OSD_INFO_FORMAT =        "osdInfoFmt";
    AdvParam RAPAM_OSD_TEXT_POSITION =      "osdTextPos";
    AdvParam RAPAM_OSD_TEXT =               "osdText";

    const char * PARAM_CHANNELS[] = {
        "0 (177/6 MHz)",
        "1 (189/6 MHz)",
        "2 (201/6 MHz)",
        "3 (213/6 MHz)",
        "4 (225/6 MHz)",
        "5 (237/6 MHz)",
        "6 (249/6 MHz)",
        "7 (261/6 MHz)",
        "8 (273/6 MHz)",
        "9 (363/6 MHz)",
        "10 (375/6 MHz)",
        "11 (387/6 MHz)",
        "12 (399/6 MHz)",
        "13 (411/6 MHz)",
        "14 (423/6 MHz)",
        "15 (473/6 MHz)"
    };

    const char * PARAM_POSITIONS[] = {
        "0 None",
        "1 Left-Top",
        "2 Left-Center",
        "3 Left-Down",
        "4 Right-Top",
        "5 Right-Center",
        "6 Right-Down"
    };

    const char * PARAM_DATE_FORMAT[] = {
        "0 D/M/Y",
        "1 M/D/Y",
        "2 Y/M/D"
    };

    const char * PARAM_TIME_FORMAT[] = {
        "0 AM/PM",
        "1 24H"
    };

    unsigned replaceSubstring(std::string& str, const std::string& from, const std::string& to)
    {
        if (from.empty())
            return 0;

        unsigned count = 0;
        size_t pos = 0;
        while ( (pos = str.find(from, 0)) != std::string::npos )
        {
            str.replace(pos, from.length(), to);
            ++count;
        }

        return count;
    }

    unsigned replaceSubstring(std::string& str, const char * from, const char * to)
    {
        if (!from || !to)
            return 0;

        std::string sFrom(from);
        std::string sTo(to);

        return replaceSubstring(str, sFrom, sTo);
    }

    unsigned replaceSubstring(std::string& str, const char * from, const AdvParam to)
    {
        return replaceSubstring(str, from, to.name());
    }
}

namespace ite
{
    const char * AdvancedSettings::getDescriptionXML()
    {
        static std::string paramsXML;

        if (! paramsXML.size()) // first time
        {
            std::string params =
            "<?xml version=\"1.0\"?> "
            "<plugin "
                "name = \"IT930X\" "
                "version = \"1\" "
                "unique_id = \"11761fbb-04f4-40c8-8213-52d9367676c6\"> "
                "<parameters>"
                    "<group name=\"Signal\"> "
                        "<param id=\"{channel}\" name=\"Channel\" dataType=\"Enumeration\" range=\"{chEnum}\" /> "
                        "<param id=\"{present}\" name=\"Signal Presence\" dataType=\"Bool\" readOnly=\"true\" /> "
                        "<param id=\"{strength}\" name=\"Signal Strength\" dataType=\"Number\" readOnly=\"true\" range=\"0,100\" /> "
                        "<param id=\"{quality}\" name=\"Signal Quality\" dataType=\"Number\" readOnly=\"true\" range=\"0,100\" /> "
                    "</group> "
                    "<group name=\"Receiver\"> "
                        "<param id=\"{rxID}\" name=\"Receiver ID\" dataType=\"Number\" readOnly=\"true\" range=\"0,255\" /> "
                        "<param id=\"{rxCompany}\" name=\"Company\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{rxModel}\" name=\"Model\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{rxDriverVer}\" name=\"Driver Version\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{rxAPIVer}\" name=\"API Version\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{rxFWVer}\" name=\"Firmware Version\" dataType=\"String\" readOnly=\"true\" /> "
                    "</group> "
                    "<group name=\"Camera\"> "
                        "<param id=\"{txID}\" name=\"Camera ID\" dataType=\"Number\" readOnly=\"true\" range=\"0,65535\" /> "
                        "<param id=\"{txHwID}\" name=\"Camera HWID\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{txCompany}\" name=\"Company\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{txModel}\" name=\"Model\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{txSerial}\" name=\"Serial Number\" dataType=\"String\" readOnly=\"true\" /> "
                        "<param id=\"{txFWVer}\" name=\"Firmware Version\" dataType=\"String\" readOnly=\"true\" /> "
                    "</group> "
                    "<group name=\"OSD\"> "
                        "<param id=\"{osdDatePos}\" name=\"Date Position\" dataType=\"Enumeration\" range=\"{positionEnum}\" /> "
                        "<param id=\"{osdDateFmt}\" name=\"Date Format\" dataType=\"Enumeration\" range=\"{dateFmtEnum}\" /> "
                        "<param id=\"{osdTimePos}\" name=\"Time Position\" dataType=\"Enumeration\" range=\"{positionEnum}\" /> "
                        "<param id=\"{osdTimeFmt}\" name=\"Time Format\" dataType=\"Enumeration\" range=\"{timeFmtEnum}\" /> "
                        "<param id=\"{osdLogoPos}\" name=\"Logo Position\" dataType=\"Enumeration\" range=\"{positionEnum}\" /> "
                        "<param id=\"{osdLogoFmt}\" name=\"Logo Format\" dataType=\"Enumeration\" range=\"0,1\" /> "
                        "<param id=\"{osdInfoPos}\" name=\"Info Position\" dataType=\"Enumeration\" range=\"{positionEnum}\" /> "
                        "<param id=\"{osdInfoFmt}\" name=\"Info Format\" dataType=\"Enumeration\" range=\"0,1\" /> "
                        "<param id=\"{osdTextPos}\" name=\"Text Position\" dataType=\"Enumeration\" range=\"{positionEnum}\" /> "
                        "<param id=\"{osdText}\" name=\"Text\" dataType=\"String\" readOnly=\"false\" /> "
                    "</group> "
                "</parameters> "
            "</plugin>";

            std::string chanEnum;
            chanEnum += PARAM_CHANNELS[0]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[1]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[2]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[3]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[4]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[5]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[6]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[7]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[8]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[9]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[10]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[11]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[12]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[13]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[14]; chanEnum += ',';
            chanEnum += PARAM_CHANNELS[15];

            std::string posEnum;
            posEnum += PARAM_POSITIONS[0]; posEnum += ',';
            posEnum += PARAM_POSITIONS[1]; posEnum += ',';
            posEnum += PARAM_POSITIONS[2]; posEnum += ',';
            posEnum += PARAM_POSITIONS[3]; posEnum += ',';
            posEnum += PARAM_POSITIONS[4]; posEnum += ',';
            posEnum += PARAM_POSITIONS[5]; posEnum += ',';
            posEnum += PARAM_POSITIONS[6];

            std::string dateFmtEnum;
            dateFmtEnum += PARAM_DATE_FORMAT[0]; dateFmtEnum += ',';
            dateFmtEnum += PARAM_DATE_FORMAT[1]; dateFmtEnum += ',';
            dateFmtEnum += PARAM_DATE_FORMAT[2];

            std::string timeFmtEnum;
            timeFmtEnum += PARAM_TIME_FORMAT[0]; timeFmtEnum += ',';
            timeFmtEnum += PARAM_TIME_FORMAT[1];

            replaceSubstring(params, "{channel}",       PARAM_SIGNAL_CHANNEL);
            replaceSubstring(params, "{present}",       PARAM_SIGNAL_PRESENT);
            replaceSubstring(params, "{strength}",      PARAM_SIGNAL_STRENGTH);
            replaceSubstring(params, "{quality}",       PARAM_SIGNAL_QUALITY);

            replaceSubstring(params, "{rxID}",          PARAM_RX_ID);
            replaceSubstring(params, "{rxCompany}",     PARAM_RX_COMPANY);
            replaceSubstring(params, "{rxModel}",       PARAM_RX_MODEL);
            replaceSubstring(params, "{rxDriverVer}",   PARAM_RX_DRIVER_VER);
            replaceSubstring(params, "{rxAPIVer}",      PARAM_RX_API_VER);
            replaceSubstring(params, "{rxFWVer}",       PARAM_RX_FW_VER);

            replaceSubstring(params, "{txID}",          PARAM_TX_ID);
            replaceSubstring(params, "{txHwID}",        PARAM_TX_HWID);
            replaceSubstring(params, "{txCompany}",     PARAM_TX_COMPANY);
            replaceSubstring(params, "{txModel}",       PARAM_TX_MODEL);
            replaceSubstring(params, "{txSerial}",      PARAM_TX_SERIAL);
            replaceSubstring(params, "{txFWVer}",       PARAM_TX_FW_VER);

            replaceSubstring(params, "{osdDatePos}",    RAPAM_OSD_DATE_POSITION);
            replaceSubstring(params, "{osdDateFmt}",    RAPAM_OSD_DATE_FORMAT);
            replaceSubstring(params, "{osdTimePos}",    RAPAM_OSD_TIME_POSITION);
            replaceSubstring(params, "{osdTimeFmt}",    RAPAM_OSD_TIME_FORMAT);
            replaceSubstring(params, "{osdLogoPos}",    RAPAM_OSD_LOGO_POSITION);
            replaceSubstring(params, "{osdLogoFmt}",    RAPAM_OSD_LOGO_FORMAT);
            replaceSubstring(params, "{osdInfoPos}",    RAPAM_OSD_INFO_POSITION);
            replaceSubstring(params, "{osdInfoFmt}",    RAPAM_OSD_INFO_FORMAT);
            replaceSubstring(params, "{osdTextPos}",    RAPAM_OSD_TEXT_POSITION);
            replaceSubstring(params, "{osdText}",       RAPAM_OSD_TEXT);

            replaceSubstring(params, "{chEnum}",        chanEnum);
            replaceSubstring(params, "{positionEnum}",  posEnum);
            replaceSubstring(params, "{dateFmtEnum}",   dateFmtEnum);
            replaceSubstring(params, "{timeFmtEnum}",   timeFmtEnum);

            // TODO: thread safety
            if (! paramsXML.size())
                paramsXML.swap(params);
        }

        return paramsXML.c_str();
    }

    int AdvancedSettings::getParamValue(const char * name, char * valueBuf, int * valueBufSize) const
    {
        if (!m_rxDev || !m_txDev)
            return nxcip::NX_OTHER_ERROR;

        std::string str;
        bool allowEmpty = false;

        // Signal
        if (PARAM_SIGNAL_CHANNEL == name)           { getChannel(str); }
        else if(PARAM_SIGNAL_PRESENT == name)       { getPresent(str); }
        else if(PARAM_SIGNAL_STRENGTH == name)      { getStrength(str); }
        else if(PARAM_SIGNAL_QUALITY == name)       { getQuality(str); }
        // Receiver
        else if(PARAM_RX_ID == name)                { getRxID(str); }
        else if(PARAM_RX_COMPANY == name)           { getRxCompany(str); allowEmpty = true; }
        else if(PARAM_RX_MODEL == name)             { getRxModel(str); allowEmpty = true;}
        else if(PARAM_RX_DRIVER_VER == name)        { getRxDriverVer(str); allowEmpty = true; }
        else if(PARAM_RX_API_VER == name)           { getRxAPIVer(str); allowEmpty = true; }
        else if(PARAM_RX_FW_VER == name)            { getRxFwVer(str); allowEmpty = true; }
        // Camera
        else if(PARAM_TX_ID == name)                { getTxID(str); }
        else if(PARAM_TX_HWID == name)              { getTxHwID(str); allowEmpty = true; }
        else if(PARAM_TX_COMPANY == name)           { getTxCompany(str); allowEmpty = true; }
        else if(PARAM_TX_MODEL == name)             { getTxModel(str); allowEmpty = true; }
        else if(PARAM_TX_SERIAL == name)            { getTxSerial(str); allowEmpty = true; }
        else if(PARAM_TX_FW_VER == name)            { getTxFwVer(str); allowEmpty = true; }
        // OSD
        else if(RAPAM_OSD_DATE_POSITION == name)    { getOsdDatePosition(str); }
        else if(RAPAM_OSD_DATE_FORMAT == name)      { getOsdDateFormat(str); }
        else if(RAPAM_OSD_TIME_POSITION == name)    { getOsdTimePosition(str); }
        else if(RAPAM_OSD_TIME_FORMAT == name)      { getOsdTimeFormat(str); }
        else if(RAPAM_OSD_LOGO_POSITION == name)    { getOsdLogoPosition(str); }
        else if(RAPAM_OSD_LOGO_FORMAT == name)      { getOsdLogoFormat(str); }
        else if(RAPAM_OSD_INFO_POSITION == name)    { getOsdInfoPosition(str); }
        else if(RAPAM_OSD_INFO_FORMAT == name)      { getOsdInfoFormat(str); }
        else if(RAPAM_OSD_TEXT_POSITION == name)    { getOsdTextPosition(str); }
        else if(RAPAM_OSD_TEXT == name)             { getOsdText(str); allowEmpty = true; }
        else
            return nxcip::NX_UNKNOWN_PARAMETER;

        if (str.size() || allowEmpty)
        {
            int strSize = str.size();
            if (*valueBufSize < strSize)
            {
                *valueBufSize = strSize;
                return nxcip::NX_MORE_DATA;
            }

            *valueBufSize = strSize;
            strncpy(valueBuf, str.c_str(), strSize);
            valueBuf[strSize] = '\0';
        }
        else
        {
            valueBufSize = 0;
            return nxcip::NX_NO_DATA;
        }

        return nxcip::NX_NO_ERROR;
    }

    int AdvancedSettings::setParamValue(CameraManager& camera, const char * name, const char * value)
    {
        if (!name || !value)
            return nxcip::NX_INVALID_PARAM_VALUE;
        if (!m_txDev)
            return nxcip::NX_OTHER_ERROR;

        bool osd = false;

        if (name[0] == '\0')
        {
            if (PARAM_ALL_CLOSE == value)
                camera.updateSettings();
        }
        else if(PARAM_SIGNAL_CHANNEL == name)
        {
            /// @warning need value in front of string
            std::string str(value);
            unsigned chan = str2num(str);
            if (chan >= TxDevice::CHANNELS_NUM)
                return nxcip::NX_INVALID_PARAM_VALUE;

            camera.setChannel(chan);
            camera.needUpdate(GROUP_SIGNAL);
        }
        else if(RAPAM_OSD_DATE_POSITION == name)    { osd = true; getPosition(value, m_txDev->osdInfo.dateEnable, m_txDev->osdInfo.datePosition); }
        else if(RAPAM_OSD_DATE_FORMAT == name)      { osd = true; getFormat(value, m_txDev->osdInfo.dateFormat); }
        else if(RAPAM_OSD_TIME_POSITION == name)    { osd = true; getPosition(value, m_txDev->osdInfo.timeEnable, m_txDev->osdInfo.timePosition); }
        else if(RAPAM_OSD_TIME_FORMAT == name)      { osd = true; getFormat(value, m_txDev->osdInfo.timeFormat); }
        else if(RAPAM_OSD_LOGO_POSITION == name)    { osd = true; getPosition(value, m_txDev->osdInfo.logoEnable, m_txDev->osdInfo.logoPosition); }
        else if(RAPAM_OSD_LOGO_FORMAT == name)      { osd = true; getFormat(value, m_txDev->osdInfo.logoOption); }
        else if(RAPAM_OSD_INFO_POSITION == name)    { osd = true; getPosition(value, m_txDev->osdInfo.detailInfoEnable, m_txDev->osdInfo.detailInfoPosition); }
        else if(RAPAM_OSD_INFO_FORMAT == name)      { osd = true; getFormat(value, m_txDev->osdInfo.detailInfoOption); }
        else if(RAPAM_OSD_TEXT_POSITION == name)    { osd = true; getPosition(value, m_txDev->osdInfo.textEnable, m_txDev->osdInfo.textPosition); }
        else if(RAPAM_OSD_TEXT == name)             { osd = true; TxDevice::str2rcStr(value, m_txDev->osdInfo.text); }
        else
            return nxcip::NX_PARAM_READ_ONLY;

        if (osd)
            camera.needUpdate(GROUP_OSD);
        return nxcip::NX_NO_ERROR;
    }

    void AdvancedSettings::getChannel(std::string& s) const
    {
        unsigned chan = m_rxDev->channel();
        if (chan < TxDevice::CHANNELS_NUM && chan != RxDevice::NOT_A_CHANNEL)
            s = PARAM_CHANNELS[chan];
    }

    void AdvancedSettings::getPosition(const char * value, uint8_t& enabled, uint8_t& position) const
    {
        /// @warning need value in front of string
        std::string str(value);
        unsigned variant = str2num(str);

        enabled = 0;
        if (variant)
        {
            enabled = 1;
            position = variant - 1;
        }
    }

    void AdvancedSettings::getFormat(const char * value, uint8_t& format) const
    {
        /// @warning need value in front of string
        std::string str(value);
        format = str2num(str);
    }

    unsigned AdvancedSettings::getPositionVariant(uint8_t enabled, uint8_t position) const
    {
        unsigned variant = 0;
        if (enabled)
            variant = 1 + position;
        return variant;
    }

    void AdvancedSettings::getOsdDatePosition(std::string& s) const
    {
        unsigned v = getPositionVariant(m_txDev->osdInfo.dateEnable, m_txDev->osdInfo.datePosition);
        s = PARAM_POSITIONS[v];
    }

    void AdvancedSettings::getOsdDateFormat(std::string& s) const
    {
        s = PARAM_DATE_FORMAT[m_txDev->osdInfo.dateFormat];
    }

    void AdvancedSettings::getOsdTimePosition(std::string& s) const
    {
        unsigned v = getPositionVariant(m_txDev->osdInfo.timeEnable, m_txDev->osdInfo.timePosition);
        s = PARAM_POSITIONS[v];
    }

    void AdvancedSettings::getOsdTimeFormat(std::string& s) const
    {
        s = PARAM_TIME_FORMAT[m_txDev->osdInfo.timeFormat];
    }

    void AdvancedSettings::getOsdLogoPosition(std::string& s) const
    {
        unsigned v = getPositionVariant(m_txDev->osdInfo.logoEnable, m_txDev->osdInfo.logoPosition);
        s = PARAM_POSITIONS[v];
    }

    void AdvancedSettings::getOsdLogoFormat(std::string& s) const
    {
        if (m_txDev->osdInfo.logoOption)
            s = "1";
        else
            s = "0";
    }

    void AdvancedSettings::getOsdInfoPosition(std::string& s) const
    {
        unsigned v = getPositionVariant(m_txDev->osdInfo.detailInfoEnable, m_txDev->osdInfo.detailInfoPosition);
        s = PARAM_POSITIONS[v];
    }

    void AdvancedSettings::getOsdInfoFormat(std::string& s) const
    {
        if (m_txDev->osdInfo.detailInfoOption)
            s = "1";
        else
            s = "0";
    }

    void AdvancedSettings::getOsdTextPosition(std::string& s) const
    {
        unsigned v = getPositionVariant(m_txDev->osdInfo.textEnable, m_txDev->osdInfo.textPosition);
        s = PARAM_POSITIONS[v];
    }

    void AdvancedSettings::getOsdText(std::string& s) const
    {
        s = TxDevice::rcStr2str(m_txDev->osdInfo.text);
    }

    //

    INIT_OBJECT_COUNTER(CameraManager)
    DEFAULT_REF_COUNTER(CameraManager)

    CameraManager::CameraManager(const nxcip::CameraInfo& info, DeviceMapper * devMapper, TxDevicePtr txDev)
    :   m_refManager(this),
        m_devMapper(devMapper),
        m_txDevice(txDev),
        m_errorStr(nullptr)
    {
        updateCameraInfo(info);

        unsigned frequency;
        unsigned short txID;
        std::vector<unsigned short> rxIDs;
        DeviceMapper::parseInfo(info, txID, frequency, rxIDs);
    }

    CameraManager::~CameraManager()
    {}

    void * CameraManager::queryInterface( const nxpl::NX_GUID& interfaceID )
    {
        if (interfaceID == nxcip::IID_BaseCameraManager3)
        {
            addRef();
            return static_cast<nxcip::BaseCameraManager3*>(this);
        }
        if (interfaceID == nxcip::IID_BaseCameraManager2)
        {
            addRef();
            return static_cast<nxcip::BaseCameraManager2*>(this);
        }
        if (interfaceID == nxcip::IID_BaseCameraManager)
        {
            addRef();
            return static_cast<nxcip::BaseCameraManager*>(this);
        }
        if (interfaceID == nxpl::IID_PluginInterface)
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }
        return nullptr;
    }

    //

    const char * CameraManager::getParametersDescriptionXML() const
    {
        return AdvancedSettings::getDescriptionXML();
    }

    int CameraManager::getParamValue(const char * paramName, char * valueBuf, int * valueBufSize) const
    {
        return AdvancedSettings(m_txDevice, m_rxDevice).getParamValue(paramName, valueBuf, valueBufSize);
    }

    int CameraManager::setParamValue(const char * paramName, const char * value)
    {
        return AdvancedSettings(m_txDevice, m_rxDevice).setParamValue(*this, paramName, value);
    }

    void CameraManager::needUpdate(unsigned group)
    {
        m_update[group] = true;
    }

    void CameraManager::updateSettings()
    {
        if (!m_rxDevice)
            return;

        if (m_update[AdvancedSettings::GROUP_OSD])
        {
            m_update[AdvancedSettings::GROUP_OSD] = false;

            m_rxDevice->updateOSD();
        }

        if (m_update[AdvancedSettings::GROUP_SIGNAL])
        {
            m_update[AdvancedSettings::GROUP_SIGNAL] = false;

            if (m_newChannel == m_rxDevice->channel())
                return;

            if (m_rxDevice->changeChannel(m_newChannel))
            {
                stopStreams(true);
                m_devMapper->forgetTx(txID());
            }
        }
    }

    //

    int CameraManager::getEncoderCount(int * encoderCount) const
    {
        *encoderCount = RxDevice::streamsCountPassive();
        return nxcip::NX_NO_ERROR;
    }

    int CameraManager::getEncoder( int encoderIndex, nxcip::CameraMediaEncoder** encoderPtr )
    {
        State state = tryLoad();
        switch (state)
        {
            case STATE_NO_CAMERA:
            case STATE_NO_RECEIVER:
            case STATE_NOT_LOCKED:
            case STATE_NOT_CONFIGURED:
            case STATE_NO_ENCODERS:
                return nxcip::NX_TRY_AGAIN;

            case STATE_READY:
            case STATE_READING:
                break;
        }

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        //if (m_encoders.empty())
        //    return nxcip::NX_TRY_AGAIN;

        if (static_cast<unsigned>(encoderIndex) >= m_encoders.size())
            return nxcip::NX_INVALID_ENCODER_NUMBER;

        m_encoders[encoderIndex]->addRef();
        *encoderPtr = m_encoders[encoderIndex].get();
        return nxcip::NX_NO_ERROR;
    }

    int CameraManager::getCameraInfo( nxcip::CameraInfo* info ) const
    {
        memcpy( info, &m_info, sizeof(m_info) );
        return nxcip::NX_NO_ERROR;
    }

    int CameraManager::getCameraCapabilities( unsigned int* capabilitiesMask ) const
    {
        *capabilitiesMask = nxcip::BaseCameraManager::nativeMediaStreamCapability |
                            nxcip::BaseCameraManager::groupMediaParamsChangeCapability |
                            nxcip::BaseCameraManager::needIFrameDetectionCapability;
        return nxcip::NX_NO_ERROR;
    }

    void CameraManager::setCredentials( const char* /*username*/, const char* /*password*/ )
    {
    }

    int CameraManager::setAudioEnabled( int /*audioEnabled*/ )
    {
        return nxcip::NX_NO_ERROR;
    }

    nxcip::CameraPtzManager* CameraManager::getPtzManager() const
    {
        return nullptr;
    }

    nxcip::CameraMotionDataProvider* CameraManager::getCameraMotionDataProvider() const
    {
        return nullptr;
    }

    nxcip::CameraRelayIOManager* CameraManager::getCameraRelayIOManager() const
    {
        return nullptr;
    }

    void CameraManager::getLastErrorString( char* errorString ) const
    {
        if (errorString)
        {
            errorString[0] = '\0';
            if (m_errorStr)
                strncpy( errorString, m_errorStr, nxcip::MAX_TEXT_LEN );
        }
    }

    int CameraManager::createDtsArchiveReader( nxcip::DtsArchiveReader** /*dtsArchiveReader*/ ) const
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int CameraManager::find( nxcip::ArchiveSearchOptions* /*searchOptions*/, nxcip::TimePeriods** /*timePeriods*/ ) const
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    int CameraManager::setMotionMask( nxcip::Picture* /*motionMask*/ )
    {
        return nxcip::NX_NOT_IMPLEMENTED;
    }

    // internals

    CameraManager::State CameraManager::checkState() const
    {
        if (! m_txDevice)
            return STATE_NO_CAMERA;

        if (! m_rxDevice)
            return STATE_NO_RECEIVER;

        if (! m_rxDevice->isLocked() || ! m_rxDevice->isReading())
            return STATE_NOT_LOCKED;

        if (m_rxDevice->isLocked() && m_rxDevice->isReading() && ! m_txDevice->ready())
            return STATE_NOT_CONFIGURED;

        if (m_txDevice->ready() && m_encoders.size() == 0)
            return STATE_NO_ENCODERS;

        if (m_openedStreams.empty())
            return STATE_READY;

        return STATE_READING;
    }

    CameraManager::State CameraManager::tryLoad()
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        State state = checkState();
        switch (state)
        {
            case STATE_NO_CAMERA:
                return state; // can do nothing

            case STATE_NO_RECEIVER:
            case STATE_NOT_LOCKED:
            {
                if (! captureAnyRxDevice())
                    break;
                //break;
            }

            case STATE_NOT_CONFIGURED:
            {
                if (! configureTx())
                    break;
                //break;
            }

            case STATE_NO_ENCODERS:
            {
                reloadMedia();
                break;
            }

            case STATE_READY:
            case STATE_READING:
                return state;
        }

        return checkState();
    }

    bool CameraManager::configureTx()
    {
        static const unsigned WAIT_MS = 60000;

        Timer timer(true);
        while (! m_txDevice->ready())
        {
            processRC(true);

            if (timer.elapsedMS() > WAIT_MS)
            {
                debug_printf("[camera] break config process (timeout). Tx: %d; Rx: %d\n",
                             txID(), m_rxDevice ? m_rxDevice->rxID() : 0xffff);
                break;
            }

            Timer::sleep(10);
        }

#if 1   // experimental
        if (m_txDevice->responsesCount() == 0)
            freeRx(true);
#endif
        return m_txDevice->ready();
    }

    void CameraManager::reloadMedia()
    {
        stopEncoders();
        if (m_rxDevice && m_rxDevice->isLocked())
        {
            initEncoders();
            if (! m_rxDevice->isReading())
                freeRx();
        }
    }

    bool CameraManager::stopIfNeedIt()
    {
        static const unsigned WAIT_TO_STOP_MS = 10000;

        if (m_stopTimer.isStarted() && m_stopTimer.elapsedMS() > WAIT_TO_STOP_MS)
            return stopStreams();

        return false;
    }

    void CameraManager::updateCameraInfo(const nxcip::CameraInfo& info)
    {
        memcpy(&m_info, &info, sizeof(nxcip::CameraInfo));
    }

    bool CameraManager::captureAnyRxDevice()
    {
        // reopen same device
        if (m_rxDevice)
        {
            if (m_rxDevice->isLocked() && m_rxDevice->txID() == txID())
            {
                m_stopTimer.stop();
                return true;
            }
            //else
            freeRx();
        }

        RxDevicePtr dev = captureFreeRxDevice();

        if (dev)
        {
            m_rxDevice = dev;
            return true;
        }

        return false;
    }

    RxDevicePtr CameraManager::captureFreeRxDevice()
    {
        RxDevicePtr best;

        std::vector<RxDevicePtr> supportedRxDevices;
        m_devMapper->getRx4Tx(txID(), supportedRxDevices);

        for (size_t i = 0; i < supportedRxDevices.size(); ++i)
        {
            RxDevicePtr dev = supportedRxDevices[i];

            bool isGood = true;
            if (dev->lockCamera(m_txDevice)) // delay inside
            {
                isGood = dev->good();
                if (isGood)
                {
                    if (!best || dev->strength() > best->strength())
                        best.swap(dev);
                }

                // unlock all but best
                if (dev)
                    dev->unlockC();
            }
            else
                debug_printf("[camera] can't lock Rx for Tx: %d\n", txID());
        }

        if (supportedRxDevices.empty())
            debug_printf("[camera] no Rx for Tx: %d\n", txID());

        return best;
    }

    void CameraManager::freeRx(bool resetRx)
    {
        if (m_rxDevice)
        {
            m_rxDevice->unlockC(resetRx);
            m_rxDevice.reset();
        }
        m_stopTimer.stop();
    }

    void CameraManager::initEncoders()
    {
        if (!m_rxDevice || !m_rxDevice->isLocked())
            return;

        unsigned streamsCount = m_rxDevice->streamsCount();
        m_encoders.reserve(streamsCount);

        for (unsigned i = 0; i < streamsCount; ++i)
        {
            nxcip::ResolutionInfo res;
            m_rxDevice->resolution(i, res.resolution.width, res.resolution.height, res.maxFps);

            if (i >= m_encoders.size())
            {
                auto enc = std::shared_ptr<MediaEncoder>( new MediaEncoder(this, i, res) );
                m_encoders.push_back(enc);
            }
            else
                m_encoders[i]->updateResolution(res);

            m_rxDevice->subscribe(i);
        }
    }

    void CameraManager::stopEncoders()
    {
        for (unsigned i = 0; i < m_rxDevice->streamsCount(); ++i)
            m_rxDevice->unsubscribe(i);
    }

    // from StreamReader thread

    /// @hack reading RC in StreamReader thread
    void CameraManager::processRC(bool update)
    {
        RxDevicePtr dev = m_rxDevice;
        if (dev)
        {
            dev->processRcQueue();

            if (update && ! m_txDevice->ready())
                dev->updateTxParams();
        }
    }

    DevReader * CameraManager::devReader() const
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        if (m_rxDevice)
            return m_rxDevice->reader();
        return nullptr;
    }

    void CameraManager::openStream(unsigned encNo)
    {
        m_stopTimer.stop();

        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        m_openedStreams.insert(encNo);
    }

    void CameraManager::closeStream(unsigned encNo)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        m_openedStreams.erase(encNo);
        if (m_openedStreams.empty())
            m_stopTimer.restart();
    }

    // from another thread

    bool CameraManager::stopStreams(bool force)
    {
        std::lock_guard<std::mutex> lock( m_mutex ); // LOCK

        if (!m_rxDevice)
            return false;

        if (m_openedStreams.size() && ! force)
            return false;

        stopEncoders();
        freeRx();
        return true;
    }
}
