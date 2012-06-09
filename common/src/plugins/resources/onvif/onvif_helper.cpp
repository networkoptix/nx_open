#include "onvif_helper.h"
#include "onvif/soapDeviceBindingProxy.h"
#include "utils/common/log.h"
#include <QDebug>


extern const char* ACTI_MANUFACTURER;
extern const char* ARECONT_VISION_MANUFACTURER;
extern const char* AVIGILON_MANUFACTURER;
extern const char* AXIS_MANUFACTURER;
extern const char* BASLER_MANUFACTURER;
extern const char* BOSH_DINION_MANUFACTURER;
extern const char* BRICKCOM_MANUFACTURER;
extern const char* CISCO_MANUFACTURER;
extern const char* DIGITALWATCHDOG_MANUFACTURER;
extern const char* DLINK_MANUFACTURER;
extern const char* DROID_MANUFACTURER;
extern const char* GRANDSTREAM_MANUFACTURER;
extern const char* HIKVISION_MANUFACTURER;
extern const char* HONEYWELL_MANUFACTURER;
extern const char* IQINVISION_MANUFACTURER;
extern const char* IPX_DDK_MANUFACTURER;
extern const char* ISD_MANUFACTURER;
extern const char* MOBOTIX_MANUFACTURER;
extern const char* PANASONIC_MANUFACTURER;
extern const char* PELCO_SARIX_MANUFACTURER;
extern const char* PIXORD_MANUFACTURER;
extern const char* PULSE_MANUFACTURER;
extern const char* SAMSUNG_MANUFACTURER;
extern const char* SANYO_MANUFACTURER;
extern const char* SCALLOP_MANUFACTURER;
extern const char* SONY_MANUFACTURER;
extern const char* STARDOT_MANUFACTURER;
extern const char* STARVEDIA_MANUFACTURER;
extern const char* TRENDNET_MANUFACTURER;
extern const char* TOSHIBA_MANUFACTURER;
extern const char* VIDEOIQ_MANUFACTURER;
extern const char* VIVOTEK_MANUFACTURER;
extern const char* UBIQUITI_MANUFACTURER;


const char* ADMIN1 = "admin";
const char* ADMIN2 = "Admin";
const char* ADMIN3 = "administrator";
const char* ROOT = "root";
const char* SUPERVISOR = "supervisor";
const char* MAIN_USER1 = "ubnt";
const char* PASSWD1 = "123456";
const char* PASSWD2 = "pass";
const char* PASSWD3 = "12345";
const char* PASSWD4 = "1234";
const char* PASSWD5 = "system";
const char* PASSWD6 = "meinsm";
const char* PASSWD7 = "4321";
const char* PASSWD8 = "1111111";
const char* PASSWD9 = "password";
const char* PASSWD10 = "";
const char* PASSWD11 = "ikwd";

//
// mDNS Manufacture Tokens
//

const char* ACTI_TOKEN_MDNS = "acti";
const char* ARECONT_VISION_TOKEN_MDNS = "arecont";
const char* AVIGILON_TOKEN_MDNS = "avigilon";
const char* AXIS_TOKEN_MDNS = "axis";
const char* BASLER_TOKEN_MDNS = "basler";
const char* BOSH_DINION_TOKEN_MDNS = "bosch";
const char* BRICKCOM_TOKEN_MDNS = "brickcom";
const char* CISCO_TOKEN_MDNS = "cisco";
const char* DIGITALWATCHDOG_TOKEN_MDNS = "watchdog";
const char* DLINK_TOKEN_MDNS = "dlink";
const char* DROID_TOKEN_MDNS = "droid";
const char* GRANDSTREAM_TOKEN_MDNS = "grand";
const char* HIKVISION_TOKEN_MDNS = "hikvision";
const char* HONEYWELL_TOKEN_MDNS = "honeywell";
const char* IQINVISION_TOKEN_MDNS = "iq";
const char* IPX_DDK_TOKEN_MDNS1 = "ipx";
const char* IPX_DDK_TOKEN_MDNS2 = "ddk";
const char* ISD_TOKEN_MDNS = "isd";
const char* MOBOTIX_TOKEN_MDNS = "mobotix";
const char* PANASONIC_TOKEN_MDNS = "panasonic";
const char* PELCO_SARIX_TOKEN_MDNS = "pelcosarix";
const char* PIXORD_TOKEN_MDNS = "pixord";
const char* PULSE_TOKEN_MDNS = "pulse";
const char* SAMSUNG_TOKEN_MDNS = "samsung";
const char* SANYO_TOKEN_MDNS = "sanyo";
const char* SCALLOP_TOKEN_MDNS = "scallop";
const char* SONY_TOKEN_MDNS = "sony";
const char* STARDOT_TOKEN_MDNS = "stardot";
const char* STARVEDIA_TOKEN_MDNS = "starvedia";
const char* TRENDNET_TOKEN_MDNS = "trendnet";
const char* TOSHIBA_TOKEN_MDNS = "toshiba";
const char* VIDEOIQ_TOKEN_MDNS = "videoiq";
const char* VIVOTEK_TOKEN_MDNS = "vivotek";
const char* UBIQUITI_TOKEN_MDNS = "ubiquiti";

//
// Ws-Discovery Manufacture Tokens
//

const char* ACTI_TOKEN_WSDD = "acti";
const char* ARECONT_VISION_TOKEN_WSDD = "arecont";
const char* AVIGILON_TOKEN_WSDD = "avigilon";
const char* AXIS_TOKEN_WSDD = "axis";
const char* BASLER_TOKEN_WSDD = "basler";
const char* BOSH_DINION_TOKEN_WSDD = "bosch";
const char* BRICKCOM_TOKEN_WSDD = "brickcom";
const char* CISCO_TOKEN_WSDD = "cisco";
const char* DIGITALWATCHDOG_TOKEN_WSDD = "watchdog";
const char* DLINK_TOKEN_WSDD = "dlink";
const char* DROID_TOKEN_WSDD = "droid";
const char* GRANDSTREAM_TOKEN_WSDD = "grand";
const char* HIKVISION_TOKEN_WSDD = "hikvision";
const char* HONEYWELL_TOKEN_WSDD = "honeywell";
const char* IQINVISION_TOKEN_WSDD = "iqin";
const char* IPX_DDK_TOKEN_WSDD1 = "ipx";
const char* IPX_DDK_TOKEN_WSDD2 = "ddk";
const char* ISD_TOKEN_WSDD = "isd";
const char* MOBOTIX_TOKEN_WSDD = "mobotix";
const char* PANASONIC_TOKEN_WSDD = "panasonic";
const char* PELCO_SARIX_TOKEN_WSDD = "pelcosarix";
const char* PIXORD_TOKEN_WSDD = "pixord";
const char* PULSE_TOKEN_WSDD = "pulse";
const char* SAMSUNG_TOKEN_WSDD = "samsung";
const char* SANYO_TOKEN_WSDD = "sanyo";
const char* SCALLOP_TOKEN_WSDD = "scallop";
const char* SONY_TOKEN_WSDD = "sony";
const char* STARDOT_TOKEN_WSDD = "stardot";
const char* STARVEDIA_TOKEN_WSDD = "starvedia";
const char* TRENDNET_TOKEN_WSDD = "trendnet";
const char* TOSHIBA_TOKEN_WSDD = "toshiba";
const char* VIDEOIQ_TOKEN_WSDD = "videoiq";
const char* VIVOTEK_TOKEN_WSDD = "vivotek";
const char* UBIQUITI_TOKEN_WSDD = "ubiquiti";

//
// Onvif Manufacture Tokens
//

const char* ACTI_TOKEN_ONVIF = "acti";
const char* ARECONT_VISION_TOKEN_ONVIF = "arecontvision";
const char* AVIGILON_TOKEN_ONVIF = "avigilon";
const char* AXIS_TOKEN_ONVIF = "axis";
const char* BASLER_TOKEN_ONVIF = "basler";
const char* BOSH_DINION_TOKEN_ONVIF = "bosch";
const char* BRICKCOM_TOKEN_ONVIF = "brickcom";
const char* CISCO_TOKEN_ONVIF = "cisco";
const char* DIGITALWATCHDOG_TOKEN_ONVIF = "watchdog";
const char* DLINK_TOKEN_ONVIF = "dlink";
const char* DROID_TOKEN_ONVIF = "droid";
const char* GRANDSTREAM_TOKEN_ONVIF = "grandstream";
const char* HIKVISION_TOKEN_ONVIF = "hikvision";
const char* HONEYWELL_TOKEN_ONVIF = "honeywell";
const char* IQINVISION_TOKEN_ONVIF = "iqin";
const char* IPX_DDK_TOKEN_ONVIF1 = "ipx";
const char* IPX_DDK_TOKEN_ONVIF2 = "ddk";
const char* ISD_TOKEN_ONVIF = "isd";
const char* MOBOTIX_TOKEN_ONVIF = "mobotix";
const char* PANASONIC_TOKEN_ONVIF = "panasonic";
const char* PELCO_SARIX_TOKEN_ONVIF = "pelcosarix";
const char* PIXORD_TOKEN_ONVIF = "pixord";
const char* PULSE_TOKEN_ONVIF = "pulse";
const char* SAMSUNG_TOKEN_ONVIF = "samsung";
const char* SANYO_TOKEN_ONVIF = "sanyo";
const char* SCALLOP_TOKEN_ONVIF = "scallop";
const char* SONY_TOKEN_ONVIF = "sony";
const char* STARDOT_TOKEN_ONVIF = "stardot";
const char* STARVEDIA_TOKEN_ONVIF = "starvedia";
const char* TRENDNET_TOKEN_ONVIF = "trendnet";
const char* TOSHIBA_TOKEN_ONVIF = "toshiba";
const char* VIDEOIQ_TOKEN_ONVIF = "videoiq";
const char* VIVOTEK_TOKEN_ONVIF = "vivotek";
const char* UBIQUITI_TOKEN_ONVIF = "ubiquiti";

//
// PasswordHelper
//

bool PasswordHelper::isNotAuthenticated(const SOAP_ENV__Fault* faultInfo)
{
    qDebug() << "PasswordHelper::isNotAuthenticated: all fault info: " << SoapErrorHelper::fetchDescription(faultInfo);

    if (faultInfo && faultInfo->SOAP_ENV__Code && faultInfo->SOAP_ENV__Code->SOAP_ENV__Subcode) {
        QString subcodeValue(faultInfo->SOAP_ENV__Code->SOAP_ENV__Subcode->SOAP_ENV__Value);
        subcodeValue = subcodeValue.toLower();
        qDebug() << "PasswordHelper::isNotAuthenticated: gathered string: " << subcodeValue;
        return subcodeValue.indexOf("notauthorized") != -1 || subcodeValue.indexOf("not permitted") != -1
                || subcodeValue.indexOf("failedauthentication") != -1 || subcodeValue.indexOf("operationprohibited") != -1;
    }

    return false;
}

PasswordHelper::PasswordHelper()
{
    setPasswordInfo(ACTI_MANUFACTURER, ADMIN1, PASSWD1);
    setPasswordInfo(ACTI_MANUFACTURER, ADMIN2, PASSWD1);

    setPasswordInfo(ARECONT_VISION_MANUFACTURER);

    setPasswordInfo(AVIGILON_MANUFACTURER, ADMIN1, ADMIN1);

    setPasswordInfo(AXIS_MANUFACTURER, ROOT, PASSWD2);

    setPasswordInfo(BASLER_MANUFACTURER, ADMIN1, ADMIN1);

    setPasswordInfo(BOSH_DINION_MANUFACTURER);

    setPasswordInfo(BRICKCOM_MANUFACTURER, ADMIN1, ADMIN1);

    setPasswordInfo(CISCO_MANUFACTURER);

    setPasswordInfo(GRANDSTREAM_MANUFACTURER, ADMIN1, ADMIN1);

    setPasswordInfo(HIKVISION_MANUFACTURER, ADMIN1, PASSWD3);

    setPasswordInfo(HONEYWELL_MANUFACTURER, ADMIN3, PASSWD4);

    setPasswordInfo(IQINVISION_MANUFACTURER, ROOT, PASSWD5);

    setPasswordInfo(IPX_DDK_MANUFACTURER, ROOT, ADMIN1);
    setPasswordInfo(IPX_DDK_MANUFACTURER, ROOT, ADMIN2);

    setPasswordInfo(MOBOTIX_MANUFACTURER, ADMIN1, PASSWD6);

    setPasswordInfo(PANASONIC_MANUFACTURER, ADMIN1, PASSWD3);

    setPasswordInfo(PELCO_SARIX_MANUFACTURER, ADMIN1, ADMIN1);

    setPasswordInfo(PIXORD_MANUFACTURER, ADMIN1, ADMIN1);

    setPasswordInfo(SAMSUNG_MANUFACTURER, ROOT, ROOT);
    setPasswordInfo(SAMSUNG_MANUFACTURER, ADMIN1, PASSWD7);
    setPasswordInfo(SAMSUNG_MANUFACTURER, ADMIN1, PASSWD8);

    setPasswordInfo(SANYO_MANUFACTURER, ADMIN1, ADMIN1);

    setPasswordInfo(SCALLOP_MANUFACTURER, ADMIN1, PASSWD9);

    setPasswordInfo(SONY_MANUFACTURER, ADMIN1, ADMIN1);

    setPasswordInfo(STARDOT_MANUFACTURER, ADMIN1, ADMIN1);

    setPasswordInfo(STARVEDIA_MANUFACTURER, ADMIN1, PASSWD10);

    setPasswordInfo(TRENDNET_MANUFACTURER, ADMIN1, ADMIN1);

    setPasswordInfo(TOSHIBA_MANUFACTURER, ROOT, PASSWD11);

    setPasswordInfo(VIDEOIQ_MANUFACTURER, SUPERVISOR, SUPERVISOR);

    setPasswordInfo(VIVOTEK_MANUFACTURER, ROOT, PASSWD10);

    setPasswordInfo(UBIQUITI_MANUFACTURER, MAIN_USER1, MAIN_USER1);

    //if (cl_log.logLevel() >= cl_logDEBUG1) {
    //    printPasswords();
    //}
}

void PasswordHelper::setPasswordInfo(const char* manufacturer, const char* login, const char* passwd)
{
    QString manufacturerStr(manufacturer);
    ManufacturerPasswords::Iterator iter = manufacturerPasswords.find(manufacturerStr);
    if (iter == manufacturerPasswords.end()) {
        iter = manufacturerPasswords.insert(manufacturerStr, PasswordList());
    }

    allPasswords.insert(QPair<const char*, const char*>(login, passwd));
    iter.value().insert(QPair<const char*, const char*>(login, passwd));
}

void PasswordHelper::setPasswordInfo(const char* manufacturer)
{
    QString manufacturerStr(manufacturer);
    ManufacturerPasswords::Iterator iter = manufacturerPasswords.find(manufacturerStr);
    if (iter == manufacturerPasswords.end()) {
        manufacturerPasswords.insert(manufacturerStr, PasswordList());
    }
}

const PasswordList& PasswordHelper::getPasswordsByManufacturer(const QString& manufacturer) const
{
    ManufacturerPasswords::ConstIterator it = manufacturer.isEmpty()? manufacturerPasswords.end():
        manufacturerPasswords.find(manufacturer);

    if (it == manufacturerPasswords.end()) {
        return allPasswords;
    }

    return it.value();
}

void PasswordHelper::printPasswords() const
{
    qDebug() << "PasswordHelper::printPasswords:";

    ManufacturerPasswords::const_iterator iter = manufacturerPasswords.begin();
    while (iter != manufacturerPasswords.end()) {
        qDebug() << "  Manufacturer: " << iter.key() << ": ";

        PasswordList::ConstIterator listIter = iter.value().begin();
        while (listIter != iter.value().end()) {
            qDebug() << "    " << listIter->first << " / " << listIter->second;
            ++listIter;
        }

        ++iter;
    }

    qDebug() << "  All passwords: ";

    PasswordList::ConstIterator listIter = allPasswords.begin();
    while (listIter != allPasswords.end()) {
        qDebug() << "    " << listIter->first << " / " << listIter->second;
        ++listIter;
    }
}

PasswordHelper::~PasswordHelper()
{

}

//
// ManufacturerHelper
//

ManufacturerHelper::ManufacturerHelper()
{
    setMdnsAliases();
    setWsddAliases();
    setOnvifAliases();
}

ManufacturerHelper::~ManufacturerHelper()
{

}

void ManufacturerHelper::setMdnsAliases()
{
    setAliases(manufacturerMdns, ACTI_TOKEN_MDNS, ACTI_MANUFACTURER);
    setAliases(manufacturerMdns, ARECONT_VISION_TOKEN_MDNS, ARECONT_VISION_MANUFACTURER);
    setAliases(manufacturerMdns, AVIGILON_TOKEN_MDNS, AVIGILON_MANUFACTURER);
    setAliases(manufacturerMdns, AXIS_TOKEN_MDNS, AXIS_MANUFACTURER);
    setAliases(manufacturerMdns, BASLER_TOKEN_MDNS, BASLER_MANUFACTURER);
    setAliases(manufacturerMdns, BOSH_DINION_TOKEN_MDNS, BOSH_DINION_MANUFACTURER);
    setAliases(manufacturerMdns, BRICKCOM_TOKEN_MDNS, BRICKCOM_MANUFACTURER);
    setAliases(manufacturerMdns, CISCO_TOKEN_MDNS, CISCO_MANUFACTURER);
    setAliases(manufacturerMdns, DIGITALWATCHDOG_TOKEN_MDNS, DIGITALWATCHDOG_MANUFACTURER);
    setAliases(manufacturerMdns, DLINK_TOKEN_MDNS, DLINK_MANUFACTURER);
    setAliases(manufacturerMdns, DROID_TOKEN_MDNS, DROID_MANUFACTURER);
    setAliases(manufacturerMdns, GRANDSTREAM_TOKEN_MDNS, GRANDSTREAM_MANUFACTURER);
    setAliases(manufacturerMdns, HIKVISION_TOKEN_MDNS, HIKVISION_MANUFACTURER);
    setAliases(manufacturerMdns, HONEYWELL_TOKEN_MDNS, HONEYWELL_MANUFACTURER);
    setAliases(manufacturerMdns, IQINVISION_TOKEN_MDNS, IQINVISION_MANUFACTURER);
    setAliases(manufacturerMdns, IPX_DDK_TOKEN_MDNS1, IPX_DDK_MANUFACTURER);
    setAliases(manufacturerMdns, IPX_DDK_TOKEN_MDNS2, IPX_DDK_MANUFACTURER);
    setAliases(manufacturerMdns, ISD_TOKEN_MDNS, ISD_MANUFACTURER);
    setAliases(manufacturerMdns, MOBOTIX_TOKEN_MDNS, MOBOTIX_MANUFACTURER);
    setAliases(manufacturerMdns, PANASONIC_TOKEN_MDNS, PANASONIC_MANUFACTURER);
    setAliases(manufacturerMdns, PELCO_SARIX_TOKEN_MDNS, PELCO_SARIX_MANUFACTURER);
    setAliases(manufacturerMdns, PIXORD_TOKEN_MDNS, PIXORD_MANUFACTURER);
    setAliases(manufacturerMdns, PULSE_TOKEN_MDNS, PULSE_MANUFACTURER);
    setAliases(manufacturerMdns, SAMSUNG_TOKEN_MDNS, SAMSUNG_MANUFACTURER);
    setAliases(manufacturerMdns, SANYO_TOKEN_MDNS, SANYO_MANUFACTURER);
    setAliases(manufacturerMdns, SCALLOP_TOKEN_MDNS, SCALLOP_MANUFACTURER);
    setAliases(manufacturerMdns, SONY_TOKEN_MDNS, SONY_MANUFACTURER);
    setAliases(manufacturerMdns, STARDOT_TOKEN_MDNS, STARDOT_MANUFACTURER);
    setAliases(manufacturerMdns, STARVEDIA_TOKEN_MDNS, STARVEDIA_MANUFACTURER);
    setAliases(manufacturerMdns, TRENDNET_TOKEN_MDNS, TRENDNET_MANUFACTURER);
    setAliases(manufacturerMdns, TOSHIBA_TOKEN_MDNS, TOSHIBA_MANUFACTURER);
    setAliases(manufacturerMdns, VIDEOIQ_TOKEN_MDNS, VIDEOIQ_MANUFACTURER);
    setAliases(manufacturerMdns, VIVOTEK_TOKEN_MDNS, VIVOTEK_MANUFACTURER);
    setAliases(manufacturerMdns, UBIQUITI_TOKEN_MDNS, UBIQUITI_MANUFACTURER);
}

void ManufacturerHelper::setWsddAliases()
{
    setAliases(manufacturerWsdd, ACTI_TOKEN_WSDD, ACTI_MANUFACTURER);
    setAliases(manufacturerWsdd, ARECONT_VISION_TOKEN_WSDD, ARECONT_VISION_MANUFACTURER);
    setAliases(manufacturerWsdd, AVIGILON_TOKEN_WSDD, AVIGILON_MANUFACTURER);
    setAliases(manufacturerWsdd, AXIS_TOKEN_WSDD, AXIS_MANUFACTURER);
    setAliases(manufacturerWsdd, BASLER_TOKEN_WSDD, BASLER_MANUFACTURER);
    setAliases(manufacturerWsdd, BOSH_DINION_TOKEN_WSDD, BOSH_DINION_MANUFACTURER);
    setAliases(manufacturerWsdd, BRICKCOM_TOKEN_WSDD, BRICKCOM_MANUFACTURER);
    setAliases(manufacturerWsdd, CISCO_TOKEN_WSDD, CISCO_MANUFACTURER);
    setAliases(manufacturerWsdd, DIGITALWATCHDOG_TOKEN_WSDD, DIGITALWATCHDOG_MANUFACTURER);
    setAliases(manufacturerWsdd, DLINK_TOKEN_WSDD, DLINK_MANUFACTURER);
    setAliases(manufacturerWsdd, DROID_TOKEN_WSDD, DROID_MANUFACTURER);
    setAliases(manufacturerWsdd, GRANDSTREAM_TOKEN_WSDD, GRANDSTREAM_MANUFACTURER);
    setAliases(manufacturerWsdd, HIKVISION_TOKEN_WSDD, HIKVISION_MANUFACTURER);
    setAliases(manufacturerWsdd, HONEYWELL_TOKEN_WSDD, HONEYWELL_MANUFACTURER);
    setAliases(manufacturerWsdd, IQINVISION_TOKEN_WSDD, IQINVISION_MANUFACTURER);
    setAliases(manufacturerWsdd, IPX_DDK_TOKEN_WSDD1, IPX_DDK_MANUFACTURER);
    setAliases(manufacturerWsdd, IPX_DDK_TOKEN_WSDD2, IPX_DDK_MANUFACTURER);
    setAliases(manufacturerWsdd, ISD_TOKEN_WSDD, ISD_MANUFACTURER);
    setAliases(manufacturerWsdd, MOBOTIX_TOKEN_WSDD, MOBOTIX_MANUFACTURER);
    setAliases(manufacturerWsdd, PANASONIC_TOKEN_WSDD, PANASONIC_MANUFACTURER);
    setAliases(manufacturerWsdd, PELCO_SARIX_TOKEN_WSDD, PELCO_SARIX_MANUFACTURER);
    setAliases(manufacturerWsdd, PIXORD_TOKEN_WSDD, PIXORD_MANUFACTURER);
    setAliases(manufacturerWsdd, PULSE_TOKEN_WSDD, PULSE_MANUFACTURER);
    setAliases(manufacturerWsdd, SAMSUNG_TOKEN_WSDD, SAMSUNG_MANUFACTURER);
    setAliases(manufacturerWsdd, SANYO_TOKEN_WSDD, SANYO_MANUFACTURER);
    setAliases(manufacturerWsdd, SCALLOP_TOKEN_WSDD, SCALLOP_MANUFACTURER);
    setAliases(manufacturerWsdd, SONY_TOKEN_WSDD, SONY_MANUFACTURER);
    setAliases(manufacturerWsdd, STARDOT_TOKEN_WSDD, STARDOT_MANUFACTURER);
    setAliases(manufacturerWsdd, STARVEDIA_TOKEN_WSDD, STARVEDIA_MANUFACTURER);
    setAliases(manufacturerWsdd, TRENDNET_TOKEN_WSDD, TRENDNET_MANUFACTURER);
    setAliases(manufacturerWsdd, TOSHIBA_TOKEN_WSDD, TOSHIBA_MANUFACTURER);
    setAliases(manufacturerWsdd, VIDEOIQ_TOKEN_WSDD, VIDEOIQ_MANUFACTURER);
    setAliases(manufacturerWsdd, VIVOTEK_TOKEN_WSDD, VIVOTEK_MANUFACTURER);
    setAliases(manufacturerWsdd, UBIQUITI_TOKEN_WSDD, UBIQUITI_MANUFACTURER);
}

void ManufacturerHelper::setOnvifAliases()
{
    setAliases(manufacturerOnvif, ACTI_TOKEN_ONVIF, ACTI_MANUFACTURER);
    setAliases(manufacturerOnvif, ARECONT_VISION_TOKEN_ONVIF, ARECONT_VISION_MANUFACTURER);
    setAliases(manufacturerOnvif, AVIGILON_TOKEN_ONVIF, AVIGILON_MANUFACTURER);
    setAliases(manufacturerOnvif, AXIS_TOKEN_ONVIF, AXIS_MANUFACTURER);
    setAliases(manufacturerOnvif, BASLER_TOKEN_ONVIF, BASLER_MANUFACTURER);
    setAliases(manufacturerOnvif, BOSH_DINION_TOKEN_ONVIF, BOSH_DINION_MANUFACTURER);
    setAliases(manufacturerOnvif, BRICKCOM_TOKEN_ONVIF, BRICKCOM_MANUFACTURER);
    setAliases(manufacturerOnvif, CISCO_TOKEN_ONVIF, CISCO_MANUFACTURER);
    setAliases(manufacturerOnvif, DIGITALWATCHDOG_TOKEN_ONVIF, DIGITALWATCHDOG_MANUFACTURER);
    setAliases(manufacturerOnvif, DLINK_TOKEN_ONVIF, DLINK_MANUFACTURER);
    setAliases(manufacturerOnvif, DROID_TOKEN_ONVIF, DROID_MANUFACTURER);
    setAliases(manufacturerOnvif, GRANDSTREAM_TOKEN_ONVIF, GRANDSTREAM_MANUFACTURER);
    setAliases(manufacturerOnvif, HIKVISION_TOKEN_ONVIF, HIKVISION_MANUFACTURER);
    setAliases(manufacturerOnvif, HONEYWELL_TOKEN_ONVIF, HONEYWELL_MANUFACTURER);
    setAliases(manufacturerOnvif, IQINVISION_TOKEN_ONVIF, IQINVISION_MANUFACTURER);
    setAliases(manufacturerOnvif, IPX_DDK_TOKEN_ONVIF1, IPX_DDK_MANUFACTURER);
    setAliases(manufacturerOnvif, IPX_DDK_TOKEN_ONVIF2, IPX_DDK_MANUFACTURER);
    setAliases(manufacturerOnvif, ISD_TOKEN_ONVIF, ISD_MANUFACTURER);
    setAliases(manufacturerOnvif, MOBOTIX_TOKEN_ONVIF, MOBOTIX_MANUFACTURER);
    setAliases(manufacturerOnvif, PANASONIC_TOKEN_ONVIF, PANASONIC_MANUFACTURER);
    setAliases(manufacturerOnvif, PELCO_SARIX_TOKEN_ONVIF, PELCO_SARIX_MANUFACTURER);
    setAliases(manufacturerOnvif, PIXORD_TOKEN_ONVIF, PIXORD_MANUFACTURER);
    setAliases(manufacturerOnvif, PULSE_TOKEN_ONVIF, PULSE_MANUFACTURER);
    setAliases(manufacturerOnvif, SAMSUNG_TOKEN_ONVIF, SAMSUNG_MANUFACTURER);
    setAliases(manufacturerOnvif, SANYO_TOKEN_ONVIF, SANYO_MANUFACTURER);
    setAliases(manufacturerOnvif, SCALLOP_TOKEN_ONVIF, SCALLOP_MANUFACTURER);
    setAliases(manufacturerOnvif, SONY_TOKEN_ONVIF, SONY_MANUFACTURER);
    setAliases(manufacturerOnvif, STARDOT_TOKEN_ONVIF, STARDOT_MANUFACTURER);
    setAliases(manufacturerOnvif, STARVEDIA_TOKEN_ONVIF, STARVEDIA_MANUFACTURER);
    setAliases(manufacturerOnvif, TRENDNET_TOKEN_ONVIF, TRENDNET_MANUFACTURER);
    setAliases(manufacturerOnvif, TOSHIBA_TOKEN_ONVIF, TOSHIBA_MANUFACTURER);
    setAliases(manufacturerOnvif, VIDEOIQ_TOKEN_ONVIF, VIDEOIQ_MANUFACTURER);
    setAliases(manufacturerOnvif, VIVOTEK_TOKEN_ONVIF, VIVOTEK_MANUFACTURER);
    setAliases(manufacturerOnvif, UBIQUITI_TOKEN_ONVIF, UBIQUITI_MANUFACTURER);
}

void ManufacturerHelper::setAliases(ManufacturerAliases& container, const char* token, const char* manufacturer)
{
    QString tokenStr(token);
    ManufacturerAliases::Iterator iter = container.find(tokenStr);
    if (iter != container.end()) {
        qWarning() << "PasswordHelper::setAliases: token '" << tokenStr << "' already added.";
        return;
    }

    container.insert(tokenStr, QString(manufacturer));
}

const QString ManufacturerHelper::manufacturerFromMdns(const QByteArray& packetData) const
{
    return manufacturerFrom(manufacturerMdns, packetData);
}

const QString ManufacturerHelper::manufacturerFromWsdd(const QByteArray& packetData) const
{
    return manufacturerFrom(manufacturerWsdd, packetData);
}

const QString ManufacturerHelper::manufacturerFromOnvif(const QByteArray& packetData) const
{
    return manufacturerFrom(manufacturerOnvif, packetData);
}

const QString ManufacturerHelper::manufacturerFrom(const ManufacturerAliases& tokens, const QByteArray& packetData) const
{
    const QByteArray data(packetData.toLower());
    ManufacturerAliases::ConstIterator iter = tokens.begin();
    while (iter != tokens.end()) {
        if (data.indexOf(iter.key()) != -1) {
            return iter.value();
        }

        ++iter;
    }

    return QString();
}

//
// SoapErrorHelper
//

const QString SoapErrorHelper::fetchDescription(const SOAP_ENV__Fault* faultInfo)
{
    if (!faultInfo) {
        qDebug() << "SoapErrorHelper::fetchDescription: fault info is null";
        return QString();
    }

    QString result("Fault Info. ");

    if (faultInfo->faultcode) {
        result += "Code: ";
        result += faultInfo->faultcode;
        result += ". ";
    }

    if (faultInfo->faultstring) {
        result += "Descr: ";
        result += faultInfo->faultstring;
        result += ". ";
    }

    if (faultInfo->faultactor) {
        result += "Factor: ";
        result += faultInfo->faultactor;
        result += ". ";
    }

    if (faultInfo->detail && faultInfo->detail->__any) {
        result += "Details: ";
        result += faultInfo->detail->__any;
        result += ". ";
    }

    if (faultInfo->SOAP_ENV__Reason && faultInfo->SOAP_ENV__Reason->SOAP_ENV__Text) {
        result += "Reason: ";
        result += faultInfo->SOAP_ENV__Reason->SOAP_ENV__Text;
        result += ". ";
    }

    if (faultInfo->SOAP_ENV__Code) {

        if (faultInfo->SOAP_ENV__Code->SOAP_ENV__Value) {
            result += "Additional: ";
            result += faultInfo->SOAP_ENV__Code->SOAP_ENV__Value;
            result += ". ";
        }

        if (faultInfo->SOAP_ENV__Code->SOAP_ENV__Subcode && faultInfo->SOAP_ENV__Code->SOAP_ENV__Subcode->SOAP_ENV__Value) {
            result += "Sub info: ";
            result += faultInfo->SOAP_ENV__Code->SOAP_ENV__Subcode->SOAP_ENV__Value;
            result += ". ";
        }
    }

    if (faultInfo->SOAP_ENV__Detail && faultInfo->SOAP_ENV__Detail->__any) {
        result += "Additional details: ";
        result += faultInfo->SOAP_ENV__Detail->__any;
        result += ". ";
    }

    return result;
}
