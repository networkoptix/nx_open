#include "ldap_manager.h"

#include "api/global_settings.h"
#include "utils/common/log.h"
#include <iostream>
#include <sstream>

#include <QtCore/QCryptographicHash>
#include <stdio.h>

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

    QnLdapFilter and(const QnLdapFilter& arg) {
        if (arg.isEmpty())
            return *this;

        return QnLdapFilter(QString(lit("(&%1%2)")).arg(toCompoundString()).arg(arg.toCompoundString()));
    }

    QString toString() const {
        return m_value;
    }

    bool isEmpty() const {
        return m_value.isEmpty();
    }

    bool isSimple() const {
        return isEmpty() || m_value[0] != QLatin1Char('(');
    }

    QString toCompoundString() const {
        if (isSimple())
            return m_value;

        return QString(lit("(%1)")).arg(m_value);
    }
private:
    QString m_value;
};

enum LdapVendor {
    ActiveDirectory,
    OpenLdap
};

struct DirectoryType {
    virtual const QString& UidAttr() const = 0;
    virtual const QString& Filter() const = 0;
};

struct ActiveDirectoryType : DirectoryType {
    const QString& UidAttr() const override {
        static QString attr(lit("sAMAccountName"));
        return attr;
    }
    const QString& Filter() const override {
        static QString attr(lit("(&(objectCategory=User)(!(userAccountControl:1.2.840.113556.1.4.803:=2)))"));
        return attr;
    }
};

struct OpenLdapType : DirectoryType {
    const QString& UidAttr() const override {
        static QString attr(lit("uid"));
        return attr;
    };
    const QString& Filter() const override {
        static QString attr(lit(""));
        return attr;
    }
};


namespace {
    static QByteArray md5(QByteArray data)
    {
        QCryptographicHash md5(QCryptographicHash::Md5);
        md5.addData(data);
        return md5.result();
    }

    static const QString MAIL(lit("mail"));
}

#ifdef Q_OS_WIN
#include <Windows.h>
#include <winldap.h>

#define QSTOCW(s) (const PWCHAR)(s.utf16())

#define FROM_WCHAR_ARRAY QString::fromWCharArray
#define DIGEST_MD5 L"DIGEST-MD5"
#define EMPTY_STR L""
#else

#define LDAP_DEPRECATED 1
#include <ldap.h>
#define QSTOCW(s) s.toUtf8().constData()
#define LdapGetLastError() errno
#define FROM_WCHAR_ARRAY QString::fromUtf8
#define DIGEST_MD5 "DIGEST-MD5"
#define EMPTY_STR ""
#define PWSTR char*
#define PWCHAR char*
#endif

#define LdapErrorStr(rc) FROM_WCHAR_ARRAY(ldap_err2string(rc))

#include <QtCore/QMap>
#include <QtCore/QBuffer>
#include <QCryptographicHash>

#include "network/authutil.h"

namespace {
    QString GetFirstValue(LDAP *ld, LDAPMessage *e, const QString& name) {
        QString result;

        PWCHAR *values = ldap_get_values(ld, e, QSTOCW(name));
        unsigned long nValues = ldap_count_values(values);
        for (unsigned long i = 0; i < nValues; i++) {
#ifdef Q_OS_WIN
            std::wstring value;
#else
            std::string value;
#endif
            value = values[i];

            if (!value.empty()) {
                result = FROM_WCHAR_ARRAY(values[i]);
                break;
            }
        }
        ldap_value_free(values);

        return result;
    }
}

QnLdapManager::QnLdapManager()
{
}

QnLdapManager::~QnLdapManager() {
}

class LdapSession {
public:
    LdapSession(const QnLdapSettings &settings);
    ~LdapSession();

    bool connect();
    bool fetchUsers(QnLdapUsers &users);
    bool testSettings();
    bool authenticateWithDigest(const QString &login, const QString &digest);
    QString getRealm();

    QString lastError() const;

private:
    bool detectLdapVendor(LdapVendor &);

    const QnLdapSettings& m_settings;
    QString m_lastError;

    std::unique_ptr<DirectoryType> m_dType;
    LDAP *m_ld;
};

LdapSession::LdapSession(const QnLdapSettings &settings)
    : m_settings(settings),
      m_ld(0)
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
        m_lastError = LdapErrorStr(LdapGetLastError());
        return false;
    }
#elif defined(Q_OS_LINUX)
    if (ldap_initialize(ld, QSTOCW(uri.toString())) != LDAP_SUCCESS)
    {
        m_lastError = LdapErrorStr(LdapGetLastError());
        return false;
    }
#endif

    int desired_version = LDAP_VERSION3;
    int rc = ldap_set_option(m_ld, LDAP_OPT_PROTOCOL_VERSION, &desired_version);
    if (rc != 0)
    {
        m_lastError = LdapErrorStr(rc);
        return false;
    }

#ifdef Q_OS_WIN
    rc = ldap_connect(m_ld, NULL); // Need to connect before SASL bind!
    if (rc != 0)
    {
        m_lastError = LdapErrorStr(rc);
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

bool LdapSession::fetchUsers(QnLdapUsers &users)
{
    int rc = ldap_simple_bind_s(m_ld, QSTOCW(m_settings.adminDn), QSTOCW(m_settings.adminPassword));
    if (rc != LDAP_SUCCESS)
    {
        m_lastError = LdapErrorStr(rc);
        return false;
    }

    LDAPMessage *result, *e;

    QString filter = QnLdapFilter(m_dType->Filter()).and(m_settings.searchFilter).toString();
    rc = ldap_search_ext_s(m_ld, QSTOCW(m_settings.searchBase), LDAP_SCOPE_SUBTREE, filter.isEmpty() ? 0 : QSTOCW(filter), NULL, 0, NULL, NULL, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &result);
    if (rc != LDAP_SUCCESS)
    {
        m_lastError = LdapErrorStr(rc);
        return false;
    }

    for (e = ldap_first_entry(m_ld, result); e != NULL; e = ldap_next_entry(m_ld, e)) {
        PWSTR dn;
        if ((dn = ldap_get_dn(m_ld, e)) != NULL) {
            QnLdapUser user;
            user.dn = FROM_WCHAR_ARRAY(dn);

            user.login = GetFirstValue(m_ld, e, m_dType->UidAttr());
            user.email = GetFirstValue(m_ld, e, MAIL);

            if (!user.login.isEmpty())
                users.append(user);
            ldap_memfree(dn);
        }
    }

    ldap_msgfree(result);

    return true;
}

bool LdapSession::testSettings()
{
    QUrl uri = m_settings.uri;

    int rc = ldap_simple_bind_s(m_ld, QSTOCW(m_settings.adminDn), QSTOCW(m_settings.adminPassword));
    if (rc != LDAP_SUCCESS)
    {
        m_lastError = LdapErrorStr(rc);
        return false;
    }

    LDAPMessage *result;

    QString filter = QnLdapFilter(m_dType->Filter()).and(m_settings.searchFilter).toString();
    rc = ldap_search_ext_s(m_ld, QSTOCW(m_settings.searchBase), LDAP_SCOPE_SUBTREE, filter.isEmpty() ? 0 : QSTOCW(filter), NULL, 0, NULL, NULL, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &result);
    if (rc != LDAP_SUCCESS)
    {
        m_lastError = LdapErrorStr(rc);
        return false;
    }

    ldap_msgfree(result);

    return true;
}

bool LdapSession::authenticateWithDigest(const QString &login, const QString &digest)
{
    QUrl uri = m_settings.uri;

    struct berval cred;
    cred.bv_len = 0;
    cred.bv_val = 0;

    // The servresp will contain the digest-challange after the first call.
    berval *servresp = NULL;
    long res;
    ldap_sasl_bind_s(m_ld, EMPTY_STR, DIGEST_MD5, &cred, NULL, NULL, &servresp);
    ldap_get_option(m_ld, LDAP_OPT_ERROR_NUMBER, &res);
    if (res != LDAP_SASL_BIND_IN_PROGRESS)
        return false;
    
    QMap<QByteArray, QByteArray> responseDictionary;
    QByteArray initialResponse(servresp->bv_val, servresp->bv_len);
    for (QByteArray line : smartSplit(initialResponse, ',')) {
        line = line.trimmed();
        int eqIndex = line.indexOf('=');
        if (eqIndex == -1)
            continue;

        responseDictionary[line.mid(0, eqIndex)] = unquoteStr(line.mid(eqIndex + 1));
    }

    if (!responseDictionary.contains("realm") || !responseDictionary.contains("nonce"))
        return false;

    QByteArray realm = responseDictionary["realm"];
    QByteArray nonce = responseDictionary["nonce"];

    // TODO: Generate cnonce
    QByteArray cnonce = "12345";
    QByteArray nc = "00000001";
    QByteArray qop = "auth";
    QByteArray digestUri = QByteArray("ldap/") + m_settings.uri.host().toLatin1();

    QMap<QByteArray, QByteArray> authDictionary;
    authDictionary["username"] = login.toUtf8();
    authDictionary["realm"] = realm;
    authDictionary["nonce"] = nonce;
    authDictionary["digest-uri"] = digestUri;
    authDictionary["cnonce"] = cnonce;
    authDictionary["nc"] = nc;
    authDictionary["qop"] = qop;

    QByteArray ha1 = md5(QByteArray::fromHex(digest.toLatin1()) + ":" + nonce + ":" + cnonce).toHex();
    QByteArray ha2 = md5(QByteArray("AUTHENTICATE:") + digestUri).toHex();
    authDictionary["response"] = md5(ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":" + qop + ":" + ha2).toHex();

    QByteArray authRequest;
    for (auto i = authDictionary.constBegin(); i != authDictionary.constEnd(); ++i) {
        if (i != authDictionary.constBegin()) {
            authRequest += ", ";
        }

        authRequest += i.key();
        authRequest += "=";
        if (i.key() != "nc" && i.key() != "qop")
            authRequest += "\"" + i.value() + "\"";
        else
            authRequest += i.value();
    }

    cred.bv_val = const_cast<char*>(authRequest.data());
    cred.bv_len = authRequest.size();

    servresp = NULL;
    ldap_sasl_bind_s(m_ld, EMPTY_STR, DIGEST_MD5, &cred, NULL, NULL, &servresp);
    ldap_get_option(m_ld, LDAP_OPT_ERROR_NUMBER, &res);

    return res == LDAP_SUCCESS;
}

QString LdapSession::getRealm()
{
    QString result;

    struct berval cred;
    cred.bv_len = 0;
    cred.bv_val = 0;

    // The servresp will contain the digest-challange after the first call.
    berval *servresp = NULL;
    long res;
    ldap_sasl_bind_s(m_ld, EMPTY_STR, DIGEST_MD5, &cred, NULL, NULL, &servresp);
    ldap_get_option(m_ld, LDAP_OPT_ERROR_NUMBER, &res);
    if (res != LDAP_SASL_BIND_IN_PROGRESS)
        return result;
    
    QMap<QByteArray, QByteArray> responseDictionary;
    QByteArray initialResponse(servresp->bv_val, servresp->bv_len);
    for (QByteArray line : smartSplit(initialResponse, ',')) {
        line = line.trimmed();
        int eqIndex = line.indexOf('=');
        if (eqIndex == -1)
            continue;

        if (line.mid(0, eqIndex) == "realm") {
            result = QString::fromLatin1(unquoteStr(line.mid(eqIndex + 1)));
            break;
        }
    }

    return result;
}

QString LdapSession::lastError() const
{
    return m_lastError;
}

bool LdapSession::detectLdapVendor(LdapVendor &vendor)
{
    int rc = ldap_simple_bind_s(m_ld, QSTOCW(m_settings.adminDn), QSTOCW(m_settings.adminPassword));
    if (rc != LDAP_SUCCESS)
    {
        m_lastError = LdapErrorStr(rc);
        return false;
    }

    LDAPMessage *result, *e;

    QString forestFunctionality(lit("forestFunctionality"));
    rc = ldap_search_ext_s(m_ld, NULL, LDAP_SCOPE_BASE, NULL, NULL, 0, NULL, NULL, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &result);
    if (rc != LDAP_SUCCESS)
    {
        m_lastError = LdapErrorStr(rc);
        return false;
    }

    bool isActiveDirectory = false;
    for (e = ldap_first_entry(m_ld, result); e != NULL; e = ldap_next_entry(m_ld, e)) {
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

bool QnLdapManager::fetchUsers(QnLdapUsers &users) {
    QnLdapSettings settings = QnGlobalSettings::instance()->ldapSettings();

    LdapSession session(settings);
    if (!session.connect())
    {
        NX_LOG( QString::fromLatin1("QnLdapManager::fetchUsers: connect(): %1").arg(session.lastError()), cl_logWARNING );
        return false;
    }

    if (!session.fetchUsers(users))
    {
        NX_LOG( QString::fromLatin1("QnLdapManager::fetchUsers: fetchUser(): %1").arg(session.lastError()), cl_logWARNING );
        return false;
    }

    return true;
}

bool QnLdapManager::testSettings(const QnLdapSettings& settings) {
    LdapSession session(settings);
    if (!session.connect())
    {
        NX_LOG( QString::fromLatin1("QnLdapManager::testSettings: connect(): %1").arg(session.lastError()), cl_logWARNING );
        return false;
    }

    if (!session.testSettings())
    {
        NX_LOG( QString::fromLatin1("QnLdapManager::testSettings: testSettings(): %1").arg(session.lastError()), cl_logWARNING );
        return false;
    }

    return true;
}

bool QnLdapManager::authenticateWithDigest(const QString &login, const QString &digest) {
    QnLdapSettings settings = QnGlobalSettings::instance()->ldapSettings();
    LdapSession session(settings);
    if (!session.connect())
    {
        NX_LOG( QString::fromLatin1("QnLdapManager::authenticateWithDigest: connect(): %1").arg(session.lastError()), cl_logWARNING );
        return false;
    }

    if (!session.authenticateWithDigest(login, digest))
    {
        NX_LOG( QString::fromLatin1("QnLdapManager::authenticateWithDigest: authenticateWithDigest(): %1").arg(session.lastError()), cl_logWARNING );
        return false;
    }

    return true;
}

QString QnLdapManager::realm() const
{
    QnLdapSettings settings = QnGlobalSettings::instance()->ldapSettings();
    LdapSession session(settings);
    if (!session.connect())
    {
        NX_LOG( QString::fromLatin1("QnLdapManager::realm: connect(): %1").arg(session.lastError()), cl_logWARNING );
        return lit("");
    }

    QString realm = session.getRealm();
    if (realm.isEmpty())
    {
        NX_LOG( QString::fromLatin1("QnLdapManager::realm: realm(): %1").arg(session.lastError()), cl_logWARNING );
    }

    return realm;
}
