
#ifdef ENABLE_ONVIF

#include "onvif_helper.h"
#include "onvif/soapDeviceBindingProxy.h"
#include "utils/common/log.h"
#include "core/resource/resource_type.h"

#include <QtCore/QDebug>
#include <QtCore/QRegExp>

//const QRegExp& UNNEEDED_CHARACTERS = *new QRegExp("[\\t\\n -]+");
const QRegExp& UNNEEDED_CHARACTERS = *new QRegExp(QLatin1String("[^\\d\\w]+"));

static const char* ACTI_MANUFACTURER = "acti";
static const char* ARECONT_VISION_MANUFACTURER = "arecontvision";
static const char* AVIGILON_MANUFACTURER = "avigilon";
static const char* AXIS_MANUFACTURER = "axis";
static const char* BASLER_MANUFACTURER = "basler";
static const char* BOSH_DINION_MANUFACTURER = "bosch";
static const char* BRICKCOM_MANUFACTURER = "brickcom";
static const char* CISCO_MANUFACTURER = "cisco";
static const char* DIGITALWATCHDOG_MANUFACTURER = "digitalwatchdog";
static const char* DLINK_MANUFACTURER = "dlink";
static const char* DROID_MANUFACTURER = "networkoptixdroid";
static const char* GRANDSTREAM_MANUFACTURER = "grandstream";
static const char* HIKVISION_MANUFACTURER = "hikvision";
static const char* HONEYWELL_MANUFACTURER = "honeywell";
static const char* IQINVISION_MANUFACTURER = "iqinvision";
static const char* IPX_DDK_MANUFACTURER = "ipxddk";
static const char* ISD_MANUFACTURER = "isd";
static const char* MOBOTIX_MANUFACTURER = "mobotix";
static const char* PANASONIC_MANUFACTURER = "panasonic";
static const char* PELCO_SARIX_MANUFACTURER = "pelcosarix";
static const char* PIXORD_MANUFACTURER = "pixord";
static const char* PULSE_MANUFACTURER = "pulse";
static const char* SAMSUNG_MANUFACTURER = "samsung";
static const char* SANYO_MANUFACTURER = "sanyo";
static const char* SCALLOP_MANUFACTURER = "scallop";
static const char* SONY_MANUFACTURER = "sony";
static const char* STARDOT_MANUFACTURER = "stardot";
static const char* STARVEDIA_MANUFACTURER = "starvedia";
static const char* TRENDNET_MANUFACTURER = "trendnet";
static const char* TOSHIBA_MANUFACTURER = "toshiba";
static const char* VIDEOIQ_MANUFACTURER = "videoiq";
static const char* VIVOTEK_MANUFACTURER = "vivotek";
static const char* UBIQUITI_MANUFACTURER = "ubiquiti";
static const char* CAMERA_MANUFACTURER = "camera";
static const char* DEFAULT_MANUFACTURER = "DEFAULT";

static const char* DVTEL_MANUFACTURER = "dvtel";
static const char* FLIR_MANUFACTURER = "flir";
static const char* SENTRY_MANUFACTURER = "sentry360";

static const char* ADMIN1 = "admin";
static const char* ADMIN2 = "Admin";
static const char* ADMIN3 = "administrator";
static const char* ROOT = "root";
static const char* SUPERVISOR = "supervisor";
static const char* MAIN_USER1 = "ubnt";
static const char* PASSWD1 = "123456";
static const char* PASSWD2 = "pass";
static const char* PASSWD3 = "12345";
static const char* PASSWD4 = "1234";
static const char* PASSWD5 = "system";
static const char* PASSWD6 = "meinsm";
static const char* PASSWD7 = "4321";
static const char* PASSWD8 = "1111111";
static const char* PASSWD9 = "password";
static const char* PASSWD10 = "";
static const char* PASSWD11 = "ikwd";
static const char* PASSWD_CAMERA = "camera";

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

    // default password list

    setPasswordInfo(DEFAULT_MANUFACTURER, "admin", "admin");
    setPasswordInfo(DEFAULT_MANUFACTURER, "root", "root");
    setPasswordInfo(DEFAULT_MANUFACTURER, "root", "admin");
    setPasswordInfo(DEFAULT_MANUFACTURER, "root", "pass");
    setPasswordInfo(DEFAULT_MANUFACTURER, "admin", "pass");

}

void PasswordHelper::setPasswordInfo(const char* manufacturer, const char* login, const char* passwd)
{
    QString manufacturerStr = QLatin1String(manufacturer);
    ManufacturerPasswords::Iterator iter = manufacturerPasswords.find(manufacturerStr);
    if (iter == manufacturerPasswords.end()) {
        iter = manufacturerPasswords.insert(manufacturerStr, PasswordList());
    }

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

    if (it == manufacturerPasswords.end())
        it = manufacturerPasswords.find(QLatin1String(DEFAULT_MANUFACTURER));

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

    for(const QnResourceTypePtr& rt: typeMap) {

        if (rt->getParentId().isNull() || rt->getManufacture() == lit("OnvifDevice"))
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

    QString normalizedCameraName = cameraName.toLower().replace(UNNEEDED_CHARACTERS, QString());
    do {
        QSet<QString>::ConstIterator it = camerasNames.constFind(normalizedCameraName);
        if (it != camerasNames.constEnd())
            return true;
        normalizedCameraName.chop(1);
    } while (normalizedCameraName.length() >= 4);
    return false;
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
