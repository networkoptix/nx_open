#include "ldap_manager.h"

#include "api/global_settings.h"
#include <nx/utils/log/log.h>
#include <iostream>
#include <sstream>

#include <QtCore/QCryptographicHash>
#include <stdio.h>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <winldap.h>

#define QSTOCW(s) (const PWCHAR)(s.utf16())

#define FROM_WCHAR_ARRAY QString::fromWCharArray
#define EMPTY_STR L""
#define LDAP_RESULT ULONG
#else

#define LDAP_DEPRECATED 1
#include <ldap.h>
#define QSTOCW(s) s.toUtf8().constData()
#define LdapGetLastError() errno
#define FROM_WCHAR_ARRAY QString::fromUtf8
#define EMPTY_STR ""
#define PWSTR char*
#define PWCHAR char*
#define LDAP_RESULT int
#endif

#define LdapErrorStr(rc) FROM_WCHAR_ARRAY(ldap_err2string(rc))

#include <QtCore/QMap>
#include <QtCore/QBuffer>
#include <QCryptographicHash>

#include "network/authutil.h"
#include <common/common_module.h>
#include <nx/utils/string.h>

namespace {
    static const int kPageSize = 1000;

    Qn::LdapResult translateErrorCode(LDAP_RESULT ldapCode)
    {
        switch(ldapCode)
        {
        case LDAP_SUCCESS:
            return Qn::Ldap_NoError;
        case LDAP_SIZELIMIT_EXCEEDED:
            return Qn::Ldap_SizeLimit;
        case LDAP_INVALID_CREDENTIALS:
            return Qn::Ldap_InvalidCredentials;
        default:
            return Qn::Ldap_Other;
        }
    }
}

#ifdef Q_OS_WIN
static BOOLEAN _cdecl VerifyServerCertificate(PLDAP Connection, PCCERT_CONTEXT *ppServerCert)
{
    Q_UNUSED(Connection)

    CertFreeCertificateContext(*ppServerCert);

    return TRUE;
}
#endif

class QnLdapFilter
{
public:
    QnLdapFilter()
    {
    }

    QnLdapFilter(const QString& str)
        : m_value(str.trimmed())
    {
    }

    QnLdapFilter operator &(const QnLdapFilter& arg)
    {
        if (isEmpty())
            return arg;

        if (arg.isEmpty())
            return *this;

        return QnLdapFilter(QString(lit("(&%1%2)")).arg(toCompoundString()).arg(arg.toCompoundString()));
    }

    operator QString() const
    {
        return toString();
    }

    QString toString() const
    {
        return m_value;
    }

    bool isEmpty() const
    {
        return m_value.isEmpty();
    }

    bool isSimple() const
    {
        return isEmpty() || m_value[0] != QLatin1Char('(');
    }

    QString toCompoundString() const
    {
        if (!isSimple())
            return m_value;

        return QString(lit("(%1)")).arg(m_value);
    }
private:
    QString m_value;
};

enum LdapVendor
    {
    ActiveDirectory,
    OpenLdap
};

struct DirectoryType
{
    virtual const QString& UidAttr() const = 0;
    virtual const QString& Filter() const = 0;
    virtual const QString& FullNameAttr() const = 0;
};

struct ActiveDirectoryType : DirectoryType
{
    const QString& UidAttr() const override
    {
        static QString attr(lit("sAMAccountName"));
        return attr;
    }

    const QString& Filter() const override
    {
        static QString attr(lit("(&(objectCategory=User)(!(userAccountControl:1.2.840.113556.1.4.803:=2)))"));
        return attr;
    }

    const QString& FullNameAttr() const
    {
        static QString attr(lit("displayName"));
        return attr;
    }
};

struct OpenLdapType : DirectoryType
{
    const QString& UidAttr() const override
    {
        static QString attr(lit("uid"));
        return attr;
    };

    const QString& Filter() const override
    {
        static QString attr(lit(""));
        return attr;
    }

    const QString& FullNameAttr() const
    {
        static QString attr(lit("gecos"));
        return attr;
    }
};


namespace
{
    static const QString MAIL(lit("mail"));
}

namespace
{
    QString GetFirstValue(LDAP *ld, LDAPMessage *e, const QString& name)
    {
        QString result;

        PWCHAR *values = ldap_get_values(ld, e, QSTOCW(name));
        unsigned long nValues = ldap_count_values(values);
        for (unsigned long i = 0; i < nValues; i++)
            {
#ifdef Q_OS_WIN
            std::wstring value;
#else
            std::string value;
#endif
            value = values[i];

            if (!value.empty())
            {
                result = FROM_WCHAR_ARRAY(values[i]);
                break;
            }
        }
        ldap_value_free(values);

        return result;
    }
}

QnLdapManager::QnLdapManager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    connect(commonModule->globalSettings(), &QnGlobalSettings::ldapSettingsChanged, this, &QnLdapManager::clearCache);
}

QnLdapManager::~QnLdapManager()
{
}

class LdapSession
{
public:
    LdapSession(const QnLdapSettings &settings);
    ~LdapSession();

    bool connect();
    bool fetchUsers(QnLdapUsers &users, const QString& additionFilter = QString());
    QString getUserDn(const QString& login);
    bool testSettings();
    Qn::AuthResult authenticate(const QString &dn, const QString &password);

    LDAP_RESULT lastErrorCode() const;
    QString lastErrorString() const;

private:
    bool detectLdapVendor(LdapVendor &);

    const QnLdapSettings& m_settings;
    LDAP_RESULT m_lastErrorCode;

    std::unique_ptr<DirectoryType> m_dType;
    LDAP *m_ld;
};

LdapSession::LdapSession(const QnLdapSettings &settings):
    m_settings(settings),
    m_ld(nullptr)
{
}

LdapSession::~LdapSession()
{
    if (m_ld != 0)
        ldap_unbind(m_ld);
}

bool LdapSession::connect()
{
    QUrl uri = m_settings.uri;

#if defined Q_OS_WIN
    if (uri.scheme() == lit("ldaps"))
        m_ld = ldap_sslinit(QSTOCW(uri.host()), uri.port(), 1);
    else
        m_ld = ldap_init(QSTOCW(uri.host()), uri.port());

    if (m_ld == 0)
    {
        m_lastErrorCode = LdapGetLastError();
        return false;
    }
#elif defined(Q_OS_LINUX)
    if (ldap_initialize(&m_ld, QSTOCW(uri.toString())) != LDAP_SUCCESS)
    {
        m_lastErrorCode = LdapGetLastError();
        return false;
    }
#endif

    int desired_version = LDAP_VERSION3;
    int rc = ldap_set_option(m_ld, LDAP_OPT_PROTOCOL_VERSION, &desired_version);
    if (rc != 0)
    {
        m_lastErrorCode = rc;
        return false;
    }

#ifdef Q_OS_WIN
    // If Windows Vista or later
    if ((LOBYTE(LOWORD(GetVersion()))) >= 6)
    {
        ldap_set_option(m_ld, LDAP_OPT_SERVER_CERTIFICATE, &VerifyServerCertificate);
    }

    rc = ldap_connect(m_ld, NULL); // Need to connect before SASL bind!
    if (rc != 0)
    {
        m_lastErrorCode = rc;
        return false;
    }
#endif

    LdapVendor vendor;
    if (!detectLdapVendor(vendor))
    {
        // error is set by detectLdapVendor
        return false;
    }

    if (vendor == LdapVendor::ActiveDirectory)
        m_dType.reset(new ActiveDirectoryType());
    else
        m_dType.reset(new OpenLdapType());


    return true;
}

QString LdapSession::getUserDn(const QString& login)
{
    QnLdapUsers users;
    bool result = fetchUsers(users, lit("%1=%2").arg(m_dType->UidAttr()).arg(login));
    if (!result || users.isEmpty())
        return QString();

    return users[0].dn;
}

bool LdapSession::fetchUsers(QnLdapUsers &users, const QString& customFilter)
{
    LDAP_RESULT rc = ldap_simple_bind_s(m_ld, QSTOCW(m_settings.adminDn), QSTOCW(m_settings.adminPassword));
    if (rc != LDAP_SUCCESS)
    {
        m_lastErrorCode = rc;
        return false;
    }

    LDAPMessage *result, *e;

    QString filter = QnLdapFilter(m_dType->Filter()) &
        (customFilter.isEmpty() ? m_settings.searchFilter : customFilter);

    berval *cookie = NULL;
    LDAPControl *pControl = NULL, *serverControls[2], **retServerControls = NULL;

    do
    {
        rc = ldap_create_page_control(m_ld, kPageSize, cookie, 1, &pControl);
        if (rc != LDAP_SUCCESS)
        {
            m_lastErrorCode = rc;
            return false;
        }

        serverControls[0] = pControl; serverControls[1] = NULL;

        rc = ldap_search_ext_s(m_ld, QSTOCW(m_settings.searchBase), LDAP_SCOPE_SUBTREE, filter.isEmpty() ? 0 : QSTOCW(filter), NULL, 0, serverControls, NULL, LDAP_NO_LIMIT,
    LDAP_NO_LIMIT, &result);
        if (rc != LDAP_SUCCESS)
        {
            if (pControl)
                ldap_control_free(pControl);

            m_lastErrorCode = rc;
            return false;
        }

        LDAP_RESULT lerrno = 0;
        PWCHAR lerrstr = NULL;

        if((rc = ldap_parse_result(m_ld, result, &lerrno,
                                            NULL, &lerrstr, NULL,
                                            &retServerControls, 0)) != LDAP_SUCCESS)
        {
            if (pControl)
                ldap_control_free(pControl);

            m_lastErrorCode = rc;
            return false;
        }

        ldap_memfree(lerrstr);

        if(cookie)
        {
            ber_bvfree(cookie);
            cookie = NULL;
        }

        LDAP_RESULT entcnt = 0;
        if((rc = ldap_parse_page_control(m_ld, retServerControls, &entcnt, &cookie)) != LDAP_SUCCESS)
        {
            if (pControl)
                ldap_control_free(pControl);

            m_lastErrorCode = rc;
            return false;
        }

        if (retServerControls)
        {
            ldap_controls_free(retServerControls);
            retServerControls = NULL;
        }

        if (pControl)
        {
            ldap_control_free(pControl);
            pControl = NULL;
        }

        for (e = ldap_first_entry(m_ld, result); e != NULL; e = ldap_next_entry(m_ld, e))
        {
            PWSTR dn;
            if ((dn = ldap_get_dn(m_ld, e)) != NULL)
            {
                QnLdapUser user;
                user.dn = FROM_WCHAR_ARRAY(dn);

                user.login = GetFirstValue(m_ld, e, m_dType->UidAttr());
                user.email = GetFirstValue(m_ld, e, MAIL);
                user.fullName = GetFirstValue(m_ld, e, m_dType->FullNameAttr());

                if (!user.login.isEmpty())
                    users.append(user);
                ldap_memfree(dn);
            }
        }

        ldap_msgfree(result);
    } while (cookie && cookie->bv_val != NULL && (strlen(cookie->bv_val) > 0));

    if(cookie)
        ber_bvfree(cookie);

    return true;
}

bool LdapSession::testSettings()
{
    QUrl uri = m_settings.uri;

    int rc = ldap_simple_bind_s(m_ld, QSTOCW(m_settings.adminDn), QSTOCW(m_settings.adminPassword));
    if (rc != LDAP_SUCCESS)
    {
        m_lastErrorCode = rc;
        return false;
    }

    LDAPMessage *result;

    QString filter = QnLdapFilter(m_dType->Filter()) & m_settings.searchFilter;
    rc = ldap_search_ext_s(m_ld, QSTOCW(m_settings.searchBase), LDAP_SCOPE_SUBTREE, filter.isEmpty() ? 0 : QSTOCW(filter), NULL, 0, NULL, NULL, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &result);
    if (rc != LDAP_SUCCESS)
    {
        m_lastErrorCode = rc;
        return false;
    }

    ldap_msgfree(result);

    return true;
}

Qn::AuthResult LdapSession::authenticate(const QString &dn, const QString &password)
{
    int rc = ldap_simple_bind_s(m_ld, QSTOCW(dn), QSTOCW(password));
    // TODO: vig, check error code to give more detailed error description
    if (rc != LDAP_SUCCESS)
    {
        m_lastErrorCode = rc;
        return Qn::Auth_WrongPassword;
    }

    return Qn::Auth_OK;
}

LDAP_RESULT LdapSession::lastErrorCode() const
{
    return m_lastErrorCode;
}

QString LdapSession::lastErrorString() const
{
    return LdapErrorStr(m_lastErrorCode);
}

bool LdapSession::detectLdapVendor(LdapVendor &vendor)
{
    int rc = ldap_simple_bind_s(m_ld, QSTOCW(m_settings.adminDn), QSTOCW(m_settings.adminPassword));
    if (rc != LDAP_SUCCESS)
    {
        m_lastErrorCode = rc;
        return false;
    }

    LDAPMessage *result, *e;

    QString forestFunctionality(lit("forestFunctionality"));
    rc = ldap_search_ext_s(m_ld, NULL, LDAP_SCOPE_BASE, NULL, NULL, 0, NULL, NULL, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &result);
    if (rc != LDAP_SUCCESS)
    {
        m_lastErrorCode = rc;
        return false;
    }

    bool isActiveDirectory = false;
    for (e = ldap_first_entry(m_ld, result); e != NULL; e = ldap_next_entry(m_ld, e))
    {
        PWCHAR *values = ldap_get_values(m_ld, e, QSTOCW(forestFunctionality));
        unsigned long nValues = ldap_count_values(values);
        if (nValues > 0)
        {
            isActiveDirectory = true;
            break;
        }
    }

    ldap_msgfree(result);

    vendor = isActiveDirectory ? LdapVendor::ActiveDirectory : LdapVendor::OpenLdap;
    return true;
}

Qn::LdapResult QnLdapManager::fetchUsers(QnLdapUsers &users, const QnLdapSettings& settings)
{
    LdapSession session(settings);
    if (!session.connect())
    {
        NX_LOG( QString::fromLatin1("QnLdapManager::fetchUsers: connect(): %1").arg(session.lastErrorString()), cl_logWARNING );
        return translateErrorCode(session.lastErrorCode());
    }

    if (!session.fetchUsers(users))
    {
        NX_LOG( QString::fromLatin1("QnLdapManager::fetchUsers: fetchUser(): %1").arg(session.lastErrorString()), cl_logWARNING );
        return translateErrorCode(session.lastErrorCode());
    }

    return Qn::Ldap_NoError;
}

Qn::LdapResult QnLdapManager::fetchUsers(QnLdapUsers &users)
{
    QnLdapSettings settings = commonModule()->globalSettings()->ldapSettings();
    return fetchUsers(users, settings);
}

Qn::AuthResult QnLdapManager::authenticate(const QString &login, const QString &password)
{
    QnLdapSettings settings = commonModule()->globalSettings()->ldapSettings();
    LdapSession session(settings);
    if (!session.connect())
    {
        NX_LOG( QString::fromLatin1("QnLdapManager::authenticate: connect(): %1").arg(session.lastErrorString()), cl_logWARNING );
        return Qn::Auth_LDAPConnectError;
    }

    QString dn;
    {
        QnMutexLocker lock(&m_cacheMutex);
        dn = m_dnCache.value(login);
    }

    if (dn.isEmpty())
    {
        dn = session.getUserDn(login);
        if (dn.isEmpty())
            return Qn::Auth_WrongLogin;

        QnMutexLocker lock(&m_cacheMutex);
        m_dnCache[login] = dn;
    }

    auto authResult = session.authenticate(dn, password);
    if (authResult != Qn::Auth_OK)
        NX_LOG( QString::fromLatin1("QnLdapManager::authenticate: authenticate(): %1").arg(session.lastErrorString()), cl_logWARNING );

    return authResult;
}

QString QnLdapManager::errorMessage(Qn::LdapResult ldapResult)
{
    switch(ldapResult)
    {
    case Qn::Ldap_SizeLimit:
        return lit("User limit exceeded. Narrow your filter or fix server configuration.");
    case Qn::Ldap_InvalidCredentials:
        return lit("Invalid credentials");
    default:
        return lit("Invalid ldap settings");
    }
}

void QnLdapManager::clearCache()
{
    QnMutexLocker lock(&m_cacheMutex);
    m_dnCache.clear();
}
