#include "ldap_manager.h"

#include <iostream>
#include <sstream>

#include <stdio.h>

#define SEARCH_FILTER_STR "(&(objectCategory=User)(!(userAccountControl:1.2.840.113556.1.4.803:=2)))"

#ifdef Q_OS_WIN
#include <Windows.h>
#include <winldap.h>
#define QSTOCW(s) const_cast<PWCHAR>(s.toStdWString().c_str())
#define SEARCH_FILTER L ## SEARCH_FILTER_STR
#define FROM_WCHAR_ARRAY QString::fromWCharArray
#define DIGEST_MD5 L"DIGEST-MD5"
#define SM_ACCOUNT_NAME L"sAMAccountName"
#define EMPTY_STR L""
#else

#define LDAP_DEPRECATED 1
#include <ldap.h>
#define QSTOCW(s) s.toUtf8().constData()
#define LdapGetLastError() errno
#define SEARCH_FILTER SEARCH_FILTER_STR
#define FROM_WCHAR_ARRAY QString::fromUtf8
#define DIGEST_MD5 "DIGEST-MD5"
#define SM_ACCOUNT_NAME "sAMAccountName"
#define EMPTY_STR ""
#define PWSTR char*
#define PWCHAR char*
#endif

#include <QtCore/QMap>
#include <QtCore/QBuffer>
#include <QCryptographicHash>

#include "network/authutil.h"


#define THROW_LDAP_EXCEPTION(text, rc) \
{ \
        std::ostringstream os; \
        os << text << ": " << ldap_err2string(rc); \
        throw LdapException(os.str().c_str()); \
}

class QnLdapManagerPrivate {
public:
	QnLdapManagerPrivate(const QString &host, int port, const QString &bindDn, const QString &password, const QString &searchBase) 
	: host(host),
	  port(port),
	  bindDn(bindDn),
	  password(password),
	  searchBase(searchBase) {
	}


    QString host;
    int port;
    QString bindDn;
    QString password;
    QString searchBase;

    LDAP *ld;

    LdapUsers users;
};

QnLdapManager::QnLdapManager(const QString &host, int port, const QString &bindDn, const QString &password, const QString &searchBase) 
	: d_ptr(new QnLdapManagerPrivate(host, port, bindDn, password, searchBase)) {
}

QnLdapManager::~QnLdapManager() {
}

void QnLdapManager::fetchUsers() {
    Q_D(QnLdapManager);

    d->ld = ldap_init(QSTOCW(d->host), d->port);
    if (d->ld == 0)
        THROW_LDAP_EXCEPTION("LdapManager::LdapManager(): ldap_init()", LdapGetLastError());

    try {
        int desired_version = LDAP_VERSION3;
        int rc = ldap_set_option(d->ld, LDAP_OPT_PROTOCOL_VERSION, &desired_version);
        if (rc != 0)
            THROW_LDAP_EXCEPTION("LdapManager::bind(): ldap_set_option(PROTOCOL_VERSION)", rc);

        rc = ldap_simple_bind_s(d->ld, QSTOCW(d->bindDn), QSTOCW(d->password));
        if (rc != LDAP_SUCCESS)
            THROW_LDAP_EXCEPTION("LdapManager::bind(): ldap_simple_bind_s()", rc);

        LdapUsers users;

        LDAPMessage *result, *e;

        const PWSTR filter = SEARCH_FILTER;
	    rc = ldap_search_ext_s(d->ld, QSTOCW(d->searchBase), LDAP_SCOPE_SUBTREE, filter, NULL, 0, NULL, NULL, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &result);
        if (rc != LDAP_SUCCESS)
            THROW_LDAP_EXCEPTION("LdapManager::bind(): ldap_search_ext_s()", rc);

        for (e = ldap_first_entry(d->ld, result); e != NULL; e = ldap_next_entry(d->ld, e)) {
            PWSTR dn;
            if ((dn = ldap_get_dn( d->ld, e )) != NULL) {
                LdapUser user;
                user.dn = FROM_WCHAR_ARRAY(dn);

                PWCHAR *values = ldap_get_values(d->ld, e, SM_ACCOUNT_NAME);
                unsigned long nValues = ldap_count_values(values);
                for (unsigned long i = 0; i < nValues; i++) {
#ifdef Q_OS_WIN
                    std::wstring value;
#else
                    std::string value;
#endif
                    value = values[i];

                    if (!value.empty()) {
                        user.login = FROM_WCHAR_ARRAY(values[i]);
                        break;
                    }
                }
                ldap_value_free(values);

                users[user.login] = user;
                ldap_memfree( dn );
            }
        }

        ldap_msgfree(result);
        d->users = users;

        ldap_unbind(d->ld);
    } catch (...) {
        ldap_unbind(d->ld);
        throw;
    }
}

static QByteArray md5(QByteArray data)
{
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(data);
    return md5.result();
}

bool QnLdapManager::authenticateWithDigest(const QString &login, const QString &digest) {
    Q_D(const QnLdapManager);

    LDAP *ld;

    ld = ldap_init(QSTOCW(d->host), LDAP_PORT);
    int desired_version = LDAP_VERSION3;
    int rc  = ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &desired_version);
    if (rc != 0)
        THROW_LDAP_EXCEPTION("LdapManager::bind(): ldap_set_option()", rc);
#ifdef Q_OS_WIN
    rc = ldap_connect(ld, NULL); // Need to connect before SASL bind!
#endif
    struct berval cred;
    cred.bv_len = 0;
    cred.bv_val = 0;

    // The servresp will contain the digest-challange after the first call.
    berval *servresp = NULL;
    long res;
    ldap_sasl_bind_s(ld, EMPTY_STR, DIGEST_MD5, &cred, NULL, NULL, &servresp);
    ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &res);
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
    QByteArray digestUri = QByteArray("ldap/") + realm;

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
    ldap_sasl_bind_s(ld, EMPTY_STR, DIGEST_MD5, &cred, NULL, NULL, &servresp);
    ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &res);

    return res == LDAP_SUCCESS;
}

QStringList QnLdapManager::users() {
    Q_D(const QnLdapManager);

    QStringList userLogins;

    for (auto key : d->users) {
        userLogins.push_back(key.first);
    }

    return userLogins;
}

QString QnLdapManager::realm() const
{
    //TODO return Active Directory realm
    return QString();
}
