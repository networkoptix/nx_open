#include "ldap_manager.h"

#include <iostream>
#include <sstream>
#include <stdio.h>

#include <QtCore/QMap>
#include <QtCore/QBuffer>
#include <QtCore/QCryptographicHash>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <winldap.h>

#define QSTOSTR(s) s.toStdWString()
#define QSTOCW(s) (const PWCHAR)(s.utf16())

#define FROM_WCHAR_ARRAY QString::fromWCharArray
#define EMPTY_STR L""
#define LDAP_RESULT ULONG
#else

#define LDAP_DEPRECATED 1
#include <ldap.h>

#define QSTOSTR(s) s.toStdString()
#define QSTOCW(s) s.toUtf8().constData()
#define LdapGetLastError() errno
#define FROM_WCHAR_ARRAY QString::fromUtf8
#define EMPTY_STR ""
#define PWSTR char*
#define PWCHAR char*
#define LDAP_RESULT int
#endif

#define LdapErrorStr(rc) FROM_WCHAR_ARRAY(ldap_err2string(rc))

#include <api/global_settings.h>
#include <common/common_module.h>
#include <network/authutil.h>
#include <nx/fusion/serialization/json.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>
#include <nx/utils/scope_guard.h>

namespace nx::vms::server {

namespace {

const int kPageSize = 1000;

LdapResult translateErrorCode(LDAP_RESULT ldapCode)
{
    switch(ldapCode)
    {
        case LDAP_SUCCESS:
            return LdapResult::NoError;
        case LDAP_SIZELIMIT_EXCEEDED:
            return LdapResult::SizeLimit;
        case LDAP_INVALID_CREDENTIALS:
            return LdapResult::InvalidCredentials;
        default:
            return LdapResult::Other;
    }
}

} // namespace

#ifdef Q_OS_WIN
static BOOLEAN _cdecl VerifyServerCertificate(PLDAP /*Connection*/, PCCERT_CONTEXT *ppServerCert)
{
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
    virtual ~DirectoryType() = default;
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

    const QString& FullNameAttr() const override
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
    }

    const QString& Filter() const override
    {
        static QString attr(lit(""));
        return attr;
    }

    const QString& FullNameAttr() const override
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

QString toString(LdapResult result)
{
    switch(result)
    {
        case LdapResult::NoError:
            return lit("OK");
        case LdapResult::SizeLimit:
            return lit("User limit exceeded. Narrow your filter or fix server configuration");
        case LdapResult::InvalidCredentials:
            return lit("Invalid credentials");
        default:
            return lit("Invalid ldap settings");
    }
}

LdapManager::LdapManager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    connect(
        globalSettings(), &QnGlobalSettings::ldapSettingsChanged,
        this, &LdapManager::clearCache);
}

LdapManager::~LdapManager()
{
}

class LdapSession
{
public:
    LdapSession(const QnLdapSettings& settings);
    ~LdapSession();

    bool connect();
    bool fetchUsers(QnLdapUsers& users, const QString& additionFilter = QString());
    QString getUserDn(const QString& login);
    bool testSettings();
    Qn::AuthResult authenticate(const QString& dn, const QString& password);

    LDAP_RESULT lastErrorCode() const;
    QString lastErrorString() const;

private:
    bool detectLdapVendor(LdapVendor&);
    void logResponseReferences(LDAPMessage* response);

    const QnLdapSettings m_settings;
    LDAP_RESULT m_lastErrorCode;

    std::unique_ptr<DirectoryType> m_dType;
    LDAP *m_ld;
};

LdapSession::LdapSession(const QnLdapSettings& settings):
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
    NX_INFO(this, "Connecting with settings %1",
        m_settings.toString(!nx::utils::log::showPasswords()));
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

    // NOTE: Chasing referrals doesn't work with paging controls, see: VMS-15694.
    rc = ldap_set_option(m_ld, LDAP_OPT_REFERRALS, LDAP_OPT_OFF);
    if (rc != 0)
    {
        m_lastErrorCode = rc;
        return false;
    }

    if (m_settings.searchTimeoutS > 0)
    {
        NX_VERBOSE(this, lm("Set time limit and timeout to %1 seconds").arg(m_settings.searchTimeoutS));
        rc = ldap_set_option(m_ld, LDAP_OPT_TIMELIMIT, &m_settings.searchTimeoutS);
        if (rc != 0)
        {
            m_lastErrorCode = rc;
            return false;
        }
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

    NX_VERBOSE(this, "Connected to vendor %1", m_dType);
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

void LdapSession::logResponseReferences(LDAPMessage* response)
{
    // NOTE: The function is only useful for debugging, let's disable it if logs are not verbose
    // for better performance.
    if (nx::utils::log::mainLogger()->maxLevel() < nx::utils::log::Level::verbose)
        return;

    for (LDAPMessage *entry = ldap_first_reference(m_ld, response);
        entry != nullptr; entry = ldap_next_reference(m_ld, entry))
    {
        PWCHAR *refs = nullptr;
        const auto memoryGuard = nx::utils::makeScopeGuard(
            [&refs, this]()
            {
                if (refs != nullptr)
                    ldap_value_free(refs);
                refs = nullptr;
            });

        #if defined Q_OS_WIN
            LDAP_RESULT rc = ldap_parse_reference(m_ld, entry, &refs);
        #else
            LDAP_RESULT rc = ldap_parse_reference(m_ld, entry, &refs, nullptr, 0);
        #endif
        if (rc != LDAP_SUCCESS)
        {
            NX_DEBUG(this, lm("Failed to parse ldap reference entry: %1").args(LdapErrorStr(rc)));
            return;
        }

        for (int i = 0; refs != nullptr && refs[i] != nullptr; i++)
            NX_VERBOSE(this, lm("Not chasing reference received from server: %1").args(refs[i]));
    }
}

bool LdapSession::fetchUsers(QnLdapUsers& users, const QString& customFilter)
{
    QString filter = QnLdapFilter(m_dType->Filter()) &
        (customFilter.isEmpty() ? m_settings.searchFilter : customFilter);
    NX_INFO(this, "Fetching users with filter [%1]'", filter);

    LDAP_RESULT rc =
        ldap_simple_bind_s(m_ld, QSTOCW(m_settings.adminDn), QSTOCW(m_settings.adminPassword));
    if (rc != LDAP_SUCCESS)
    {
        m_lastErrorCode = rc;
        return false;
    }

    berval *cookie = NULL;
    const auto cleanUpCookie =
        [&]()
        {
            if (cookie)
                ber_bvfree(cookie);
            cookie = NULL;
        };

    LDAPMessage* result = NULL;
    LDAPControl* pControl = NULL;
    LDAPControl* serverControls[2] = {NULL, NULL};
    LDAPControl** retServerControls = NULL;
    PWCHAR lerrstr = NULL;
    const auto cleanUpControls =
        [&]()
        {
            if (result)
                ldap_msgfree(result);
            result = NULL;

            if (pControl)
                ldap_control_free(pControl);
            pControl = NULL;

            serverControls[0] = NULL;
            serverControls[1] = NULL;

            if (retServerControls)
                ldap_controls_free(retServerControls);
            retServerControls = NULL;

            if (lerrstr)
                ldap_memfree(lerrstr);
            lerrstr = NULL;
        };

    const auto memoryGuard = nx::utils::makeScopeGuard(
        [&]()
        {
            cleanUpCookie();
            cleanUpControls();
        });

    auto uidAttr = QSTOSTR(m_dType->UidAttr());
    auto nameAttr = QSTOSTR(m_dType->FullNameAttr());
    auto mailAttr = QSTOSTR(MAIL);
    PWCHAR attrs[] = {uidAttr.data(), nameAttr.data(), mailAttr.data(), NULL};

    do
    {
        cleanUpControls();
        rc = ldap_create_page_control(m_ld, kPageSize, cookie, 1, &pControl);
        if (rc != LDAP_SUCCESS)
        {
            m_lastErrorCode = rc;
            return false;
        }

        serverControls[0] = pControl;
        serverControls[1] = NULL;
        rc = ldap_search_ext_s(
            /* ld */ m_ld,
            /* base */ QSTOCW(m_settings.searchBase),
            /* scope */ LDAP_SCOPE_SUBTREE,
            /* filter */ filter.isEmpty() ? 0 : QSTOCW(filter),
            /* attrs */ attrs,
            /* attrsonly */ 0,
            /* serverctrls */ serverControls,
            /* clientctrls */ NULL,
            /* timeout */ NULL,
            /* sizelimit */ LDAP_NO_LIMIT,
            /* res */ &result);
        if (rc != LDAP_SUCCESS)
        {
            m_lastErrorCode = rc;
            return false;
        }

        LDAP_RESULT lerrno = 0;
        rc = ldap_parse_result(m_ld, result, &lerrno, NULL, &lerrstr, NULL, &retServerControls, 0);
        if (rc != LDAP_SUCCESS)
        {
            NX_ASSERT(lerrno == rc, lm("lerrno (%1) != rc (%2)").args(lerrno, rc));
            m_lastErrorCode = rc;
            return false;
        }
        NX_ASSERT(lerrno == LDAP_SUCCESS, lm("lerrno: %1").args(lerrno));

        LDAP_RESULT entcnt = 0;
        cleanUpCookie();
        if((rc = ldap_parse_page_control(
            m_ld, retServerControls, &entcnt, &cookie)) != LDAP_SUCCESS)
        {
            m_lastErrorCode = rc;
            return false;
        }
        // NOTE: Most of the time is zero, LDAP servers doesn't like to provide it =(
        NX_VERBOSE(this, lm("Entities received on page: %1").args(entcnt));

        LDAPMessage *entry = NULL;
        for (entry = ldap_first_entry(m_ld, result);
             entry != NULL;
             entry = ldap_next_entry(m_ld, entry))
        {
            PWSTR dn;
            if ((dn = ldap_get_dn(m_ld, entry)) != NULL)
            {
                QnLdapUser user;
                user.dn = FROM_WCHAR_ARRAY(dn);

                user.login = GetFirstValue(m_ld, entry, m_dType->UidAttr());
                user.email = GetFirstValue(m_ld, entry, MAIL);
                user.fullName = GetFirstValue(m_ld, entry, m_dType->FullNameAttr());

                if (!user.login.isEmpty())
                    users.append(user);
                else
                    NX_DEBUG(this, lm("Ignoring entry with empty login: %1").args(user.dn));
                ldap_memfree(dn);
            }
            else
            {
                // NOTE: it may be changed to ldap_get_option(ld, LDAP_OPT_RESULT_CODE, &err)
                // TODO: Can LdapErrorStr be used here? Is LdapGetLastError another value?
                NX_DEBUG(this, lm("Failed to extract DN of LDAP entry: %1").args(
                    LdapErrorStr(LdapGetLastError())));
            }
        }

        logResponseReferences(result);

        NX_VERBOSE(this, lm("Fetched page with [%1] return code. Currently fetched: %2 users").args(
            LdapErrorStr(LdapGetLastError()), users.size()));
        NX_VERBOSE(this, lm("cookie: %1, cookie->bv_val: %2, length: %3 (%4)").args(
            cookie != nullptr,
            cookie != nullptr ? (cookie->bv_val != nullptr) : false,
            (cookie != nullptr && cookie->bv_val != nullptr) ? strlen(cookie->bv_val) : -7,
            (cookie != nullptr && cookie->bv_val != nullptr) ? cookie->bv_len : -7));
    } while (cookie && cookie->bv_val != NULL && cookie->bv_len > 0);

    NX_INFO(this, lm("Fetched %1 user(s)%2").args(
        users.size(), users.size() < 10 ? " - " + containerString(users) : QString()));

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
    rc = ldap_search_ext_s(
        /* ld */ m_ld,
        /* base */ QSTOCW(m_settings.searchBase),
        /* scope */ LDAP_SCOPE_SUBTREE,
        /* filter */ filter.isEmpty() ? 0 : QSTOCW(filter),
        /* attrs */ NULL,
        /* attrsonly */ 0,
        /* serverctrls */ NULL,
        /* clientctrls */ NULL,
        /* timeout */ NULL,
        /* sizelimit */ LDAP_NO_LIMIT,
        /* res */ &result);
    if (rc != LDAP_SUCCESS)
    {
        m_lastErrorCode = rc;
        return false;
    }

    ldap_msgfree(result);
    return true;
}

Qn::AuthResult LdapSession::authenticate(const QString& dn, const QString& password)
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

bool LdapSession::detectLdapVendor(LdapVendor& vendor)
{
    int rc = ldap_simple_bind_s(m_ld, QSTOCW(m_settings.adminDn), QSTOCW(m_settings.adminPassword));
    if (rc != LDAP_SUCCESS)
    {
        m_lastErrorCode = rc;
        return false;
    }

    LDAPMessage *result, *e;

    QString forestFunctionality(lit("forestFunctionality"));
    rc = ldap_search_ext_s(
        /* ld */ m_ld,
        /* base */ NULL,
        /* scope */ LDAP_SCOPE_BASE,
        /* filter */ NULL,
        /* attrs */ NULL,
        /* attrsonly */ 0,
        /* serverctrls */ NULL,
        /* clientctrls */ NULL,
        /* timeout */ NULL,
        /* sizelimit */ LDAP_NO_LIMIT,
        /* res */ &result);
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
        ldap_value_free(values);
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

LdapResult LdapManager::fetchUsers(QnLdapUsers& users, const QnLdapSettings& settings)
{
    LdapSession session(settings);
    if (!session.connect())
    {
        NX_WARNING(this, lm("LDAP connect failed: %1").arg(session.lastErrorString()));
        return translateErrorCode(session.lastErrorCode());
    }

    if (!session.fetchUsers(users))
    {
        NX_WARNING(this, lm("Users fetch failed: %1").arg(session.lastErrorString()));
        return translateErrorCode(session.lastErrorCode());
    }

    return LdapResult::NoError;
}

LdapResult LdapManager::fetchUsers(QnLdapUsers& users)
{
    QnLdapSettings settings = commonModule()->globalSettings()->ldapSettings();
    return fetchUsers(users, settings);
}

Qn::AuthResult LdapManager::authenticate(const QString& login, const QString& password)
{
    QnLdapSettings settings = commonModule()->globalSettings()->ldapSettings();
    LdapSession session(settings);
    if (!session.connect())
    {
        NX_WARNING(this, lm("connect: %1").arg(session.lastErrorString()));
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
        {
            NX_INFO(this, lm("User not found, LDAP code: %1").args(session.lastErrorString()));
            return Qn::Auth_WrongLogin;
        }

        QnMutexLocker lock(&m_cacheMutex);
        m_dnCache[login] = dn;
    }

    auto authResult = session.authenticate(dn, password);
    if (authResult != Qn::Auth_OK)
        NX_WARNING(this, lm("Authentication failed: %1").arg(session.lastErrorString()));

    return authResult;
}

void LdapManager::clearCache()
{
    QnMutexLocker lock(&m_cacheMutex);
    m_dnCache.clear();
}

} // namespace nx::vms::server
