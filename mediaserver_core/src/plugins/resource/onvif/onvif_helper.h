#ifndef onvif_helper_h
#define onvif_helper_h

#ifdef ENABLE_ONVIF

#include <list>

#include <QHash>
#include <QtCore/QString>
#include <QtCore/QSet>
#include <QtCore/QPair>
#include <nx/utils/singleton.h>

//first - login, second - password
typedef std::list<std::pair<const char*, const char*> > PasswordList;

struct SOAP_ENV__Fault;

class PasswordHelper:
    public QObject,
    public Singleton<PasswordHelper>
{
    //QHash<Manufacturer, Passwords>
    typedef QHash<QString, PasswordList> ManufacturerPasswords;

    ManufacturerPasswords manufacturerPasswords;

public:
    PasswordHelper();
    ~PasswordHelper() {};

    static bool isNotAuthenticated(const SOAP_ENV__Fault* faultInfo);
    static bool isConflictError(const SOAP_ENV__Fault* faultInfo);

    const PasswordList getPasswordsByManufacturer(const QString& mdnsPacketData) const;

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
