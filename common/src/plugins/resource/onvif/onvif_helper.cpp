
#ifdef ENABLE_ONVIF

#include "onvif_helper.h"
#include "onvif/soapDeviceBindingProxy.h"
#include "utils/common/log.h"
#include "core/resource/resource_type.h"

#include <QtCore/QDebug>
#include <QtCore/QRegExp>

//const QRegExp& UNNEEDED_CHARACTERS = *new QRegExp("[\\t\\n -]+");
const QRegExp& UNNEEDED_CHARACTERS = *new QRegExp(QLatin1String("[^\\d\\w]+"));

const char* PasswordHelper::ACTI_MANUFACTURER = "acti";
const char* PasswordHelper::ARECONT_VISION_MANUFACTURER = "arecontvision";
const char* PasswordHelper::AVIGILON_MANUFACTURER = "avigilon";
const char* PasswordHelper::AXIS_MANUFACTURER = "axis";
const char* PasswordHelper::BASLER_MANUFACTURER = "basler";
const char* PasswordHelper::BOSH_DINION_MANUFACTURER = "bosch";
const char* PasswordHelper::BRICKCOM_MANUFACTURER = "brickcom";
const char* PasswordHelper::CISCO_MANUFACTURER = "cisco";
const char* PasswordHelper::DIGITALWATCHDOG_MANUFACTURER = "digitalwatchdog";
const char* PasswordHelper::DLINK_MANUFACTURER = "dlink";
const char* PasswordHelper::DROID_MANUFACTURER = "networkoptixdroid";
const char* PasswordHelper::GRANDSTREAM_MANUFACTURER = "grandstream";
const char* PasswordHelper::HIKVISION_MANUFACTURER = "hikvision";
const char* PasswordHelper::HONEYWELL_MANUFACTURER = "honeywell";
const char* PasswordHelper::IQINVISION_MANUFACTURER = "iqinvision";
const char* PasswordHelper::IPX_DDK_MANUFACTURER = "ipxddk";
const char* PasswordHelper::ISD_MANUFACTURER = "isd";
const char* PasswordHelper::MOBOTIX_MANUFACTURER = "mobotix";
const char* PasswordHelper::PANASONIC_MANUFACTURER = "panasonic";
const char* PasswordHelper::PELCO_SARIX_MANUFACTURER = "pelcosarix";
const char* PasswordHelper::PIXORD_MANUFACTURER = "pixord";
const char* PasswordHelper::PULSE_MANUFACTURER = "pulse";
const char* PasswordHelper::SAMSUNG_MANUFACTURER = "samsung";
const char* PasswordHelper::SANYO_MANUFACTURER = "sanyo";
const char* PasswordHelper::SCALLOP_MANUFACTURER = "scallop";
const char* PasswordHelper::SONY_MANUFACTURER = "sony";
const char* PasswordHelper::STARDOT_MANUFACTURER = "stardot";
const char* PasswordHelper::STARVEDIA_MANUFACTURER = "starvedia";
const char* PasswordHelper::TRENDNET_MANUFACTURER = "trendnet";
const char* PasswordHelper::TOSHIBA_MANUFACTURER = "toshiba";
const char* PasswordHelper::VIDEOIQ_MANUFACTURER = "videoiq";
const char* PasswordHelper::VIVOTEK_MANUFACTURER = "vivotek";
const char* PasswordHelper::UBIQUITI_MANUFACTURER = "ubiquiti";
const char* PasswordHelper::CAMERA_MANUFACTURER = "camera";

const char* PasswordHelper::DVTEL_MANUFACTURER = "dvtel";
const char* PasswordHelper::FLIR_MANUFACTURER = "flir";
const char* PasswordHelper::SENTRY_MANUFACTURER = "sentry360";

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
const char* PASSWD_CAMERA = "camera";

//
// PasswordHelper
//

PasswordHelper& PasswordHelper::instance()
{
    static PasswordHelper inst;
    return inst;
}

bool PasswordHelper::isNotAuthenticated(const SOAP_ENV__Fault* faultInfo)
{
#ifdef ONVIF_DEBUG
    qDebug() << "PasswordHelper::isNotAuthenticated: all fault info: " << SoapErrorHelper::fetchDescription(faultInfo);
#endif

    if (!faultInfo) {
        return false;
    }

    QString info;

    if (faultInfo->SOAP_ENV__Code && faultInfo->SOAP_ENV__Code->SOAP_ENV__Subcode) {
        info = QString::fromLatin1(faultInfo->SOAP_ENV__Code->SOAP_ENV__Subcode->SOAP_ENV__Value);
    } else if (faultInfo->faultstring) {
        info = QString::fromLatin1(faultInfo->faultstring);
    }

    info = info.toLower();
#ifdef ONVIF_DEBUG
    qDebug() << "PasswordHelper::isNotAuthenticated: gathered string: " << info;
#endif

    return info.indexOf(QLatin1String("notauthorized")) != -1 ||
        info.indexOf(QLatin1String("not permitted")) != -1 ||
        info.indexOf(QLatin1String("failedauthentication")) != -1 ||
        info.indexOf(QLatin1String("operationprohibited")) != -1 ||
        info.indexOf(QLatin1String("unauthorized")) != -1;
}

bool PasswordHelper::isConflictError(const SOAP_ENV__Fault* faultInfo)
{
    if (faultInfo && faultInfo->SOAP_ENV__Reason && faultInfo->SOAP_ENV__Reason->SOAP_ENV__Text)
    {
        QString reasonValue(QLatin1String(faultInfo->SOAP_ENV__Reason->SOAP_ENV__Text));
        reasonValue = reasonValue.toLower();

        return reasonValue.indexOf(QLatin1String("conflict")) != -1;
    }

    return false;
}

PasswordHelper::PasswordHelper()
{
    setPasswordInfo(ACTI_MANUFACTURER, ADMIN1, PASSWD1);
    setPasswordInfo(ACTI_MANUFACTURER, ADMIN2, PASSWD1);

    setPasswordInfo(ARECONT_VISION_MANUFACTURER);

    setPasswordInfo(AVIGILON_MANUFACTURER, ADMIN1, ADMIN1);

    setPasswordInfo(AXIS_MANUFACTURER, ROOT, ROOT);
    setPasswordInfo(AXIS_MANUFACTURER, ROOT, PASSWD2);

    setPasswordInfo(BASLER_MANUFACTURER, ADMIN1, ADMIN1);

    setPasswordInfo(BOSH_DINION_MANUFACTURER);

    setPasswordInfo(BRICKCOM_MANUFACTURER, ADMIN1, ADMIN1);

    setPasswordInfo(CISCO_MANUFACTURER);

    setPasswordInfo(DIGITALWATCHDOG_MANUFACTURER, ADMIN1, ADMIN1);

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

    setPasswordInfo(CAMERA_MANUFACTURER, ROOT, PASSWD_CAMERA);

    setPasswordInfo(DVTEL_MANUFACTURER,  ADMIN1, PASSWD4);
    setPasswordInfo(FLIR_MANUFACTURER,   ADMIN1, "fliradmin");
    setPasswordInfo(SENTRY_MANUFACTURER, ADMIN1, PASSWD4);

    setPasswordInfo("vista", "admin", "admin");
    setPasswordInfo("norbain", "admin", "admin");
    setPasswordInfo("vista_", "admin", "admin");
    setPasswordInfo("norbain_", "admin", "admin");

    //if (cl_log.logLevel() >= cl_logDEBUG1) {
    //    printPasswords();
    //}
}

void PasswordHelper::setPasswordInfo(const char* manufacturer, const char* login, const char* passwd)
{
    QString manufacturerStr = QLatin1String(manufacturer);
    ManufacturerPasswords::Iterator iter = manufacturerPasswords.find(manufacturerStr);
    if (iter == manufacturerPasswords.end()) {
        iter = manufacturerPasswords.insert(manufacturerStr, PasswordList());
    }

    allPasswords.insert(QPair<const char*, const char*>(login, passwd));
    iter.value().insert(QPair<const char*, const char*>(login, passwd));
}

void PasswordHelper::setPasswordInfo(const char* manufacturer)
{
    QString manufacturerStr = QLatin1String(manufacturer);
    ManufacturerPasswords::Iterator iter = manufacturerPasswords.find(manufacturerStr);
    if (iter == manufacturerPasswords.end()) {
        manufacturerPasswords.insert(manufacturerStr, PasswordList());
    }
}

const PasswordList& PasswordHelper::getPasswordsByManufacturer(const QString& manufacturer) const
{
#ifdef ONVIF_DEBUG
    qDebug() << "PasswordHelper::getPasswordsByManufacturer: manufacturer: " << manufacturer
        << ", normalized: " << manufacturer.toLower().replace(UNNEEDED_CHARACTERS, QString());
#endif
    ManufacturerPasswords::ConstIterator it = manufacturer.isEmpty()? manufacturerPasswords.end():
        manufacturerPasswords.find(manufacturer.toLower().replace(UNNEEDED_CHARACTERS, QString()));

    if (it == manufacturerPasswords.end()) {
        return allPasswords;
    }

    return it.value();
}

void PasswordHelper::printPasswords() const
{
#ifdef ONVIF_DEBUG
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
#endif
}

//
// SoapErrorHelper
//

const QString SoapErrorHelper::fetchDescription(const SOAP_ENV__Fault* faultInfo)
{
    if (!faultInfo) {
#ifdef ONVIF_DEBUG
        qDebug() << "SoapErrorHelper::fetchDescription: fault info is null";
#endif
        return lit("unknown_error"); // TODO: #Elric #TR
    }

    QByteArray result("Fault Info. ");

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

    return QLatin1String(result);
}

//
// NameHelper
//

NameHelper::NameHelper()
{
    QnResourceTypePool::QnResourceTypeMap typeMap = qnResTypePool->getResourceTypeMap();

    foreach(QnResourceTypePtr rt, typeMap) {

        if (rt->getParentId().isNull())
            continue;

        QString normalizedManufacturer = rt->getManufacture().toLower().replace(UNNEEDED_CHARACTERS, QString());
        QString normalizedName = rt->getName().toLower().replace(UNNEEDED_CHARACTERS, QString());

        if (normalizedName == normalizedManufacturer + QString(lit("camera")))
            continue;

        normalizedName = normalizedName.replace(normalizedManufacturer, QString());

        camerasNames.insert(normalizedName);
    }
}

NameHelper& NameHelper::instance()
{
    static NameHelper inst;
    return inst;
}

bool NameHelper::isSupported(const QString& cameraName) const
{
    if (cameraName.isEmpty()) {
        return false;
    }

    //qDebug() << "NameHelper::isSupported: camera name: " << cameraName << ", normalized: "
    //         << cameraName.toLower().replace(UNNEEDED_CHARACTERS, QString());

    QSet<QString>::ConstIterator it = camerasNames.constFind(cameraName.toLower().replace(UNNEEDED_CHARACTERS, QString()));
    if (it == camerasNames.constEnd()) {
        //qDebug() << "NameHelper::isSupported: can't find " << cameraName;
        return false;
    }

    //qDebug() << "NameHelper::isSupported: " << cameraName << " found";
    return true;
}

bool NameHelper::isManufacturerSupported(const QString& manufacturer) const
{
    QString tmp = manufacturer.toLower().replace(UNNEEDED_CHARACTERS, QString());
    if (tmp == QLatin1String("sony") ||
        tmp == QLatin1String("brickcom") ||
        //tmp == QLatin1String(AXIS_MANUFACTURER) ||
        tmp == QLatin1String("digitalwatchdog"))
    {
        return false;
    }

    return true;
}

#endif //ENABLE_ONVIF
