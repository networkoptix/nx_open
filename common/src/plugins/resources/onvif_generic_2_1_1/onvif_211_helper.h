#ifndef onvif_2_1_1_helper_h
#define onvif_2_1_1_helper_h

#include <QHash>
#include <QString>
#include <QSet>
#include <QPair>

//first - login, second - password
typedef QSet<QPair<const char*, const char*> > PasswordList;

class SOAP_ENV__Fault;

class PasswordHelper {
    //QHash<Manufacturer in lower case, Passwords>
    typedef QHash<QString, PasswordList> ManufacturerPasswords;

    PasswordList allPasswords;
    ManufacturerPasswords manufacturerPasswords;

    PasswordHelper(const PasswordHelper&) {}

public:

    static bool isNotAuthenticated(const SOAP_ENV__Fault* faultInfo);

    PasswordHelper();
    ~PasswordHelper();

    const PasswordList& getPasswordsByManufacturer(const QByteArray& mdnsPacketData) const;

private:

    void setPasswordInfo(const char* manufacturer, const char* login, const char* passwd);
    void setPasswordInfo(const char* manufacturer);
    void printPasswords() const;
};

class SoapErrorHelper {
    SoapErrorHelper(){}

public:
    static const QString fetchDescription(const SOAP_ENV__Fault* faultInfo);
};

#endif // onvif_2_1_1_helper_h
