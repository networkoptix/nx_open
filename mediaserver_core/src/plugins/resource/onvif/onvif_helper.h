#ifndef onvif_helper_h
#define onvif_helper_h

#ifdef ENABLE_ONVIF

#include <QHash>
#include <QtCore/QString>
#include <QtCore/QSet>
#include <QtCore/QPair>

//first - login, second - password
typedef QSet<QPair<const char*, const char*> > PasswordList;

struct SOAP_ENV__Fault;

class PasswordHelper
{
    //QHash<Manufacturer, Passwords>
    typedef QHash<QString, PasswordList> ManufacturerPasswords;

    ManufacturerPasswords manufacturerPasswords;

    PasswordHelper(const PasswordHelper&) {}
    PasswordHelper();
    ~PasswordHelper() {};

public:

    static PasswordHelper& instance();

    static bool isNotAuthenticated(const SOAP_ENV__Fault* faultInfo);
    static bool isConflictError(const SOAP_ENV__Fault* faultInfo);

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
    bool isManufacturerSupported(const QString& manufacturer) const;
};

#endif //ENABLE_ONVIF

#endif // onvif_helper_h
