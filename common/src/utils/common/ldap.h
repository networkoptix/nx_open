#ifndef QN_LDAP_H
#define QN_LDAP_H

struct QnLdapSettings {
    QnLdapSettings() {}

    QString host;
    int port;
    QString adminDn;
    QString adminPassword;
    QString searchBase;

    bool equals(const QnLdapSettings &other) const;
    bool isValid() const;

    static int defaultPort() { return 389; }
};

#endif // QN_LDAP_H