#include "onvif_211_helper.h"
#include "onvif/soapDeviceBindingProxy.h"
#include "utils/common/log.h"
#include <QDebug>

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

const char* ACTI_MANUFACTURER = "acti";
const char* ARECONT_VISION_MANUFACTURER = "arecontvision";
const char* AVIGILON_MANUFACTURER = "avigilon";
const char* AXIS_MANUFACTURER = "axis";
const char* BASLER_MANUFACTURER = "basler";
const char* BOSH_DINION_MANUFACTURER = "bosch";
const char* BRICKCOM_MANUFACTURER = "brickcom";
const char* CISCO_MANUFACTURER = "cisco";
const char* GRANDSTREAM_MANUFACTURER = "grandstream";
const char* HIKVISION_MANUFACTURER = "hikvision";
const char* HONEYWELL_MANUFACTURER = "honeywell";
const char* IQINVISION_MANUFACTURER = "iqin";
const char* IPX_DDK_MANUFACTURER = "ipxddk";
const char* MOBOTIX_MANUFACTURER = "mobotix";
const char* PANASONIC_MANUFACTURER = "panasonic";
const char* PELCO_SARIX_MANUFACTURER = "pelcosarix";
const char* PIXORD_MANUFACTURER = "pixord";
const char* SAMSUNG_MANUFACTURER = "samsung";
const char* SANYO_MANUFACTURER = "sanyo";
const char* SCALLOP_MANUFACTURER = "scallop";
const char* SONY_MANUFACTURER = "sony";
const char* STARDOT_MANUFACTURER = "stardot";
const char* STARVEDIA_MANUFACTURER = "starvedia";
const char* TRENDNET_MANUFACTURER = "trendnet";
const char* TOSHIBA_MANUFACTURER = "toshiba";
const char* VIDEOIQ_MANUFACTURER = "videoiq";
const char* VIVOTEK_MANUFACTURER = "vivotek";
const char* UBIQUITI_MANUFACTURER = "ubiquiti";


bool PasswordHelper::isNotAuthenticated(const SOAP_ENV__Fault* faultInfo)
{
    if (faultInfo && faultInfo->SOAP_ENV__Code && faultInfo->SOAP_ENV__Code->SOAP_ENV__Subcode) {
        QString subcodeValue(faultInfo->SOAP_ENV__Code->SOAP_ENV__Subcode->SOAP_ENV__Value);
        subcodeValue = subcodeValue.toLower();
        qDebug() << "PasswordHelper::isNotAuthenticated: gathered string: " << subcodeValue;
        return subcodeValue.indexOf("notauthorized") != -1 || subcodeValue.indexOf("not permitted") != -1
                || subcodeValue.indexOf("operationprohibited") != -1;
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
    setPasswordInfo(AXIS_MANUFACTURER, ROOT, "admin123"); //ToDo: delete
    setPasswordInfo(AXIS_MANUFACTURER, "admin", "admin"); //ToDo: delete

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

PasswordHelper::~PasswordHelper()
{

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

const PasswordList& PasswordHelper::getPasswordsByManufacturer(const QByteArray& mdnsPacketData) const
{
    const QByteArray data(mdnsPacketData.toLower());
    ManufacturerPasswords::const_iterator iter = manufacturerPasswords.begin();
    while (iter != manufacturerPasswords.end()) {
        if (data.indexOf(iter.key()) != -1) {
            //qDebug() << "PasswordHelper::getPasswordsByManufacturer: manufacturer was found: " << iter.key();
            return iter.value();
        }

        ++iter;
    }

    //qDebug() << "PasswordHelper::getPasswordsByManufacturer: manufacturer was not found";
    return allPasswords;
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
