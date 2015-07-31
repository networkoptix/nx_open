// ldap.cpp : Defines the entry point for the console application.
//

#include "ldap_manager.h"

#include <iostream>
#include <sstream>

#include <stdio.h>

#include <Windows.h>
#include <winldap.h>

#define QSTOCW(s) const_cast<PWCHAR>(s.toStdWString().c_str())

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
    QString searchBase;
    QString bindDn;
    QString password;

    LDAP *ld;

    LdapUsers users;
};

/*
std::wstring Utf8ToUtf16(const std::string &s)
{
    std::wstring ret;
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), s.length(), NULL, 0);
    if (len > 0) 
    {
      ret.resize(len);
      MultiByteToWideChar(CP_UTF8, 0, s.c_str(), s.length(), const_cast<wchar_t*>(ret.c_str()), len);
    }
    return ret;
}

std::string Utf16ToUtf8(const std::wstring &s)
{
    QString ret;
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), s.length(), NULL, 0, NULL, NULL);
    if (len > 0)
    {
      ret.resize(len);
      WideCharToMultiByte(CP_UTF8, 0, s.c_str(), s.length(), const_cast<char*>(ret.c_str()), len, NULL, NULL);
    }
    return ret;
}
*/

QnLdapManager::QnLdapManager(const QString &host, int port, const QString &bindDn, const QString &password, const QString &searchBase) 
	: d_ptr(new QnLdapManagerPrivate(host, port, bindDn, password, searchBase)) {

    Q_D(QnLdapManager);

    d->ld = ldap_init(QSTOCW(d->host), d->port);
    if (d->ld == 0)
        THROW_LDAP_EXCEPTION("LdapManager::LdapManager(): ldap_init()", LdapGetLastError());

    int desired_version = LDAP_VERSION3;
    int rc = ldap_set_option(d->ld, LDAP_OPT_PROTOCOL_VERSION, &desired_version);
    if (rc != 0)
        THROW_LDAP_EXCEPTION("LdapManager::bind(): ldap_set_option(PROTOCOL_VERSION)", rc);

    bind();
}

QnLdapManager::~QnLdapManager() {
    Q_D(QnLdapManager);

    ldap_unbind(d->ld);
}

void QnLdapManager::bind() {
    Q_D(QnLdapManager);

    int rc = ldap_simple_bind_s(d->ld, QSTOCW(d->bindDn), QSTOCW(d->password));
    if (rc != LDAP_SUCCESS)
        THROW_LDAP_EXCEPTION("LdapManager::bind(): ldap_simple_bind_s()", rc);
}

void QnLdapManager::fetchUsers() {
    Q_D(QnLdapManager);

    LdapUsers users;

    LDAPMessage *result, *e;

    const PWSTR filter = L"(&(objectCategory=User)(!(userAccountControl:1.2.840.113556.1.4.803:=2)))";
	int rc = ldap_search_ext_s(d->ld, QSTOCW(d->searchBase), LDAP_SCOPE_SUBTREE, filter, NULL, 0, NULL, NULL, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &result);
    if (rc != LDAP_SUCCESS)
        THROW_LDAP_EXCEPTION("LdapManager::bind(): ldap_search_ext_s()", rc);

    for (e = ldap_first_entry(d->ld, result); e != NULL; e = ldap_next_entry(d->ld, e)) {
        PWSTR dn;
        if ((dn = ldap_get_dn( d->ld, e )) != NULL) {
            LdapUser user;
            user.dn = QString::fromWCharArray(dn);

            PWCHAR *values = ldap_get_values(d->ld, e, L"sAMAccountName");
            ULONG nValues = ldap_count_values(values);
            for (ULONG i = 0; i < nValues; i++) {
                std::wstring value = values[i];
                if (!value.empty()) {
                    user.login = QString::fromWCharArray(values[i]);
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
}

QStringList QnLdapManager::users() {
    Q_D(const QnLdapManager);

    QStringList userLogins;

    for (auto key : d->users) {
        userLogins.push_back(key.first);
    }

    return userLogins;
}

bool QnLdapManager::verifyCredentials(const QString &login, const QString &password) {
    Q_D(QnLdapManager);

    if (login.isEmpty() || password.isEmpty())
        return false;

    if (d->users.empty())
        fetchUsers();

    auto loginIter = d->users.find(login);
    if (loginIter == d->users.end())
        return false;

	QString dn = loginIter->second.dn;
    LDAP *ld2;
    ld2 = ldap_init(QSTOCW(d->host), d->port);
    if (ld2 == 0)
        THROW_LDAP_EXCEPTION("LdapManager::LdapManager(): ldap_init()", LdapGetLastError());


    int desired_version = LDAP_VERSION3;
    int rc = ldap_set_option(ld2, LDAP_OPT_PROTOCOL_VERSION, &desired_version);
    if (rc != 0)
        THROW_LDAP_EXCEPTION("LdapManager::bind(): ldap_set_option()", rc);

    rc = ldap_simple_bind_s(ld2, QSTOCW(dn), QSTOCW(password));
    if (rc != LDAP_SUCCESS)
        return false;

/*  LDAPMessage *result;
    rc = ldap_search_ext_s(d->ld, L"dc=corp,dc=hdw,dc=mx", LDAP_SCOPE_SUBTREE, NULL, NULL, 0, NULL, NULL, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &result);
    if (rc != LDAP_SUCCESS)
        return false; */

    ldap_unbind(ld2);
    return true;
    /* ULONG has_value = ldap_compare_s(d->ld, L"cn=Test User,cn=Users,dc=corp,dc=hdw,dc=mx", L"userPassword", L"QWEasd123"); 
    switch ( has_value ) { 
        case LDAP_COMPARE_TRUE: 
            printf( "Works.\n"); 
            break; 
        case LDAP_COMPARE_FALSE: 
            printf( "Failed.\n"); 
            break; 
        default: 
            THROW_LDAP_EXCEPTION("LdapManager::verifyCredentials(): ldap_compare_s(): ", LdapGetLastError());
    } */

    
/*    int rc = ldap_search_ext_s(d->ld, L"dc=corp,dc=hdw,dc=mx", LDAP_SCOPE_SUBTREE, filter, NULL, 0, NULL, NULL, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &result);
    if (rc != LDAP_SUCCESS) {
    } */

//    return false;
}

/*
try {
    LdapManager ldapManager(NX_LDAP_HOST, NX_LDAP_PORT, LDAP_ADMIN_DN, LDAP_ADMIN_PW, LDAP_SEARCH_BASE);
    return ldapManager.verifyCredentials(login, password);
} catch (const LdapException& e) {
    return false;
}
*/