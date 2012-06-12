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
    const char* ACTI_MANUFACTURER = "Acti";
    const char* ARECONT_VISION_MANUFACTURER = "ArecontVision";
    const char* AVIGILON_MANUFACTURER = "Avigilon";
    const char* AXIS_MANUFACTURER;
    const char* BASLER_MANUFACTURER;
    const char* BOSH_DINION_MANUFACTURER;
    const char* BRICKCOM_MANUFACTURER;
    const char* CISCO_MANUFACTURER;
    const char* DIGITALWATCHDOG_MANUFACTURER;
    const char* DLINK_MANUFACTURER;
    const char* DROID_MANUFACTURER;
    const char* GRANDSTREAM_MANUFACTURER;
    const char* HIKVISION_MANUFACTURER;
    const char* HONEYWELL_MANUFACTURER;
    const char* IQINVISION_MANUFACTURER;
    const char* IPX_DDK_MANUFACTURER;
    const char* ISD_MANUFACTURER;
    const char* MOBOTIX_MANUFACTURER;
    const char* PANASONIC_MANUFACTURER;
    const char* PELCO_SARIX_MANUFACTURER;
    const char* PIXORD_MANUFACTURER;
    const char* PULSE_MANUFACTURER;
    const char* SAMSUNG_MANUFACTURER;
    const char* SANYO_MANUFACTURER;
    const char* SCALLOP_MANUFACTURER;
    const char* SONY_MANUFACTURER;
    const char* STARDOT_MANUFACTURER;
    const char* STARVEDIA_MANUFACTURER;
    const char* TRENDNET_MANUFACTURER;
    const char* TOSHIBA_MANUFACTURER;
    const char* VIDEOIQ_MANUFACTURER;
    const char* VIVOTEK_MANUFACTURER;
    const char* UBIQUITI_MANUFACTURER;

    const QRegExp& WHITE_SPACES;

    //QHash<Manufacturer, Passwords>
    typedef QHash<QString, PasswordList> ManufacturerPasswords;

    PasswordList allPasswords;
    ManufacturerPasswords manufacturerPasswords;

    PasswordHelper(const PasswordHelper&) {}

public:

    static bool isNotAuthenticated(const SOAP_ENV__Fault* faultInfo);

    PasswordHelper();
    ~PasswordHelper();

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

#endif // onvif_helper_h
