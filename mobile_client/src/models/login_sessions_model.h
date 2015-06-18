#ifndef QNLOGINSESSIONSMODEL_H
#define QNLOGINSESSIONSMODEL_H

#include <QtCore/QAbstractListModel>

class QnModuleFinder;
class SocketAddress;
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

    Q_FLAGS(DisplayMode DisplayModeFlags)

    Q_PROPERTY(DisplayModeFlags displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)

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

    enum DisplayMode {
        ShowSaved = 0x1,
        ShowDiscovered = 0x2,
        ShowAll = ShowSaved | ShowDiscovered
    };

    Q_DECLARE_FLAGS(DisplayModeFlags, DisplayMode)

    QnLoginSessionsModel(QObject *parent = 0);
    ~QnLoginSessionsModel();

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    void resetSessions(const QList<QnLoginSession> &savedSessions, const QList<QnLoginSession> &discoveredSessions);

    DisplayModeFlags displayMode() const;
    void setDisplayMode(DisplayModeFlags displayMode);

public slots:
    void updateSession(const QString &address, const int port, const QString &user, const QString &password, const QString &systemName);
    void deleteSession(const QString &id);

signals:
    void displayModeChanged();

private:
    void at_moduleFinder_moduleAddressFound(const QnModuleInformation &moduleInformation, const SocketAddress &address);
    void at_moduleFinder_moduleAddressLost(const QnModuleInformation &moduleInformation, const SocketAddress &address);

private:
    int savedSessionIndex(int row) const;
    int discoveredSessionIndex(int row) const;
    int savedSessionRow(int index) const;
    int discoveredSessionRow(int index) const;
    QnLoginSession session(int row) const;

    void loadFromSettings();
    void saveToSettings();

private:
    DisplayModeFlags m_displayMode;
    QnModuleFinder *m_moduleFinder;

    QList<QnLoginSession> m_savedSessions;
    QList<QnLoginSession> m_discoveredSessions;
};

#endif // QNLOGINSESSIONSMODEL_H
