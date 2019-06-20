#pragma once

#ifdef ENABLE_ONVIF

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QSet>

#include <utils/common/credentials.h>

struct SOAP_ENV__Fault;

using CredentialsList = QList<nx::vms::common::Credentials>;

class PasswordHelper: public QObject
{
    Q_OBJECT

    using ManufacturerCredentials = QHash<QString, CredentialsList>;

    ManufacturerCredentials manufacturerPasswords;

public:
    PasswordHelper();
    ~PasswordHelper() = default;

    static bool isNotAuthenticated(const SOAP_ENV__Fault* faultInfo);
    static bool isConflict(const SOAP_ENV__Fault* faultInfo);

    const CredentialsList getCredentialsByManufacturer(const QString& mdnsPacketData) const;

private:
    void setPasswordInfo(const char* manufacturer, const char* login, const char* passwd);
    void setPasswordInfo(const char* manufacturer);
    void printPasswords() const;
};

class SoapErrorHelper
{
public:
    static const QString fetchDescription(const SOAP_ENV__Fault* faultInfo);
};

class NameHelper
{
    std::set<QString> m_camerasNames;

    NameHelper();
    NameHelper(const NameHelper&) = default;
    ~NameHelper() = default;

public:

    static NameHelper& instance();

    bool isSupported(const QString& cameraName) const;
    bool isManufacturerSupported(const QString& manufacturer) const;
};

#endif //ENABLE_ONVIF
