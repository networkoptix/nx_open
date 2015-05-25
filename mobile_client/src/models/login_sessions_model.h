#ifndef QNLOGINSESSIONSMODEL_H
#define QNLOGINSESSIONSMODEL_H

#include <QtCore/QAbstractListModel>

class QnMulticastModuleFinder;
class QnNetworkAddress;
struct QnModuleInformation;

struct QnLoginSession {
    QString systemName;
    QString address;
    int port;
    QString user;
    QString password;

    QnLoginSession() : port(-1) {}

    QVariantMap toVariant() const;
    static QnLoginSession fromVariant(const QVariantMap &variant);
    QString id() const;
};

class QnLoginSessionsModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        SystemNameRole = Qt::UserRole + 1,
        AddressRole,
        PortRole,
        UserRole,
        PasswordRole,
        SectionRole,
        SessionIdRole
    };

    QnLoginSessionsModel(QObject *parent = 0);
    ~QnLoginSessionsModel();

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    void resetSessions(const QList<QnLoginSession> &savedSessions, const QList<QnLoginSession> &discoveredSessions);

public slots:
    void updateSession(const QString &address, const int port, const QString &user, const QString &password, const QString &systemName);
    void deleteSession(const QString &id);

private:
    void at_moduleFinder_moduleAddressFound(const QnModuleInformation &moduleInformation, const QnNetworkAddress &address);
    void at_moduleFinder_moduleAddressLost(const QnModuleInformation &moduleInformation, const QnNetworkAddress &address);

private:
    int savedSessionIndex(int row) const;
    int discoveredSessionIndex(int row) const;
    QnLoginSession session(int row) const;

    void loadFromSettings();
    void saveToSettings();

private:
    QScopedPointer<QnMulticastModuleFinder> m_moduleFinder;

    QList<QnLoginSession> m_savedSessions;
    QList<QnLoginSession> m_discoveredSessions;
};

#endif // QNLOGINSESSIONSMODEL_H
