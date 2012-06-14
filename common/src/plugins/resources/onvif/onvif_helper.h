#ifndef onvif_helper_h
#define onvif_helper_h

#include <QHash>
#include <QString>
#include <QSet>
#include <QPair>

//first - login, second - password
typedef QSet<QPair<const char*, const char*> > PasswordList;

class SOAP_ENV__Fault;

class PasswordHelper
{
    static const char* ACTI_MANUFACTURER;
    static const char* ARECONT_VISION_MANUFACTURER;
    static const char* AVIGILON_MANUFACTURER;
    static const char* AXIS_MANUFACTURER;
    static const char* BASLER_MANUFACTURER;
    static const char* BOSH_DINION_MANUFACTURER;
    static const char* BRICKCOM_MANUFACTURER;
    static const char* CISCO_MANUFACTURER;
    static const char* DIGITALWATCHDOG_MANUFACTURER;
    static const char* DLINK_MANUFACTURER;
    static const char* DROID_MANUFACTURER;
    static const char* GRANDSTREAM_MANUFACTURER;
    static const char* HIKVISION_MANUFACTURER;
    static const char* HONEYWELL_MANUFACTURER;
    static const char* IQINVISION_MANUFACTURER;
    static const char* IPX_DDK_MANUFACTURER;
    static const char* ISD_MANUFACTURER;
    static const char* MOBOTIX_MANUFACTURER;
    static const char* PANASONIC_MANUFACTURER;
    static const char* PELCO_SARIX_MANUFACTURER;
    static const char* PIXORD_MANUFACTURER;
    static const char* PULSE_MANUFACTURER;
    static const char* SAMSUNG_MANUFACTURER;
    static const char* SANYO_MANUFACTURER;
    static const char* SCALLOP_MANUFACTURER;
    static const char* SONY_MANUFACTURER;
    static const char* STARDOT_MANUFACTURER;
    static const char* STARVEDIA_MANUFACTURER;
    static const char* TRENDNET_MANUFACTURER;
    static const char* TOSHIBA_MANUFACTURER;
    static const char* VIDEOIQ_MANUFACTURER;
    static const char* VIVOTEK_MANUFACTURER;
    static const char* UBIQUITI_MANUFACTURER;

    //QHash<Manufacturer, Passwords>
    typedef QHash<QString, PasswordList> ManufacturerPasswords;

    PasswordList allPasswords;
    ManufacturerPasswords manufacturerPasswords;

    PasswordHelper(const PasswordHelper&) {}
    PasswordHelper();
    ~PasswordHelper() {};

public:

    static PasswordHelper& instance();

    static bool isNotAuthenticated(const SOAP_ENV__Fault* faultInfo);;

    const PasswordList& getPasswordsByManufacturer(const QString& mdnsPacketData) const;

private:

    void setPasswordInfo(const char* manufacturer, const char* login, const char* passwd);
    void setPasswordInfo(const char* manufacturer);
    void printPasswords() const;
};

class SoapErrorHelper
{
    SoapErrorHelper(){}

public:
    static const QString fetchDescription(const SOAP_ENV__Fault* faultInfo);
};

class NameHelper
{
    QSet<QString> camerasNames;

    NameHelper(const NameHelper&) {}
    NameHelper();
    ~NameHelper() {};

public:

    static NameHelper& instance();

    bool isSupported(const QString& cameraName) const;
};

#endif // onvif_helper_h
