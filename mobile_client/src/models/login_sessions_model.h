#ifndef QNLOGINSESSIONSMODEL_H
#define QNLOGINSESSIONSMODEL_H

#include <QtCore/QAbstractListModel>

struct QnLoginSession {
    QString systemName;
    QString address;
    int port;
    QString user;
    QString password;

    QnLoginSession() : port(-1) {}

    QVariantMap toVariant() const;
    static QnLoginSession fromVariant(const QVariantMap &variant);
};

class QnLoginSessionsModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        SystemNameRole = Qt::UserRole + 1,
        AddressRole,
        PortRole,
        UserRole,
        PasswordRole
    };

    QnLoginSessionsModel(QObject *parent = 0);
    ~QnLoginSessionsModel();

    virtual int rowCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    void resetSessions(const QList<QnLoginSession> &savedSessions, const QList<QnLoginSession> &discoveredSessions);

public slots:
    void updateSession(const QString &address, const int port, const QString &user, const QString &password, const QString &systemName);

private:
    int savedSessionIndex(int row) const;
    int discoveredSessionIndex(int row) const;
    QnLoginSession session(int row) const;

    void loadFromSettings();
    void saveToSettings();

private:
    QList<QnLoginSession> m_savedSessions;
    QList<QnLoginSession> m_discoveredSessions;
};

#endif // QNLOGINSESSIONSMODEL_H
