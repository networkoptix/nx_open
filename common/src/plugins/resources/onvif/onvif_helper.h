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

class ManufacturerHelper
{
    typedef QHash<QString, QString> ManufacturerAliases;

    ManufacturerAliases manufacturerMdns;
    ManufacturerAliases manufacturerWsdd;
    ManufacturerAliases manufacturerOnvif;

public:

    ManufacturerHelper();
    ~ManufacturerHelper();

    const QString manufacturerFromMdns(const QByteArray& packetData) const;
    const QString manufacturerFromWsdd(const QByteArray& packetData) const;
    const QString manufacturerFromOnvif(const QByteArray& packetData) const;

private:

    void setMdnsAliases();
    void setWsddAliases();
    void setOnvifAliases();
    void setAliases(ManufacturerAliases& container, const char* token, const char* manufacturer);
    const QString manufacturerFrom(const ManufacturerAliases& tokens, const QByteArray& packetData) const;
};

class SoapErrorHelper
{
    SoapErrorHelper(){}

public:
    static const QString fetchDescription(const SOAP_ENV__Fault* faultInfo);
};

#endif // onvif_helper_h
