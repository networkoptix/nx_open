#pragma once

#include "login_sessions_model.h"
#include <context/login_session.h>
#include <utils/common/id.h>

class QnModuleFinder;
struct QnModuleInformation;
class SocketAddress;

class QnDiscoveredSessionsModel : public QnLoginSessionsModel
{
    Q_OBJECT

public:
    enum Roles {
        IsCompatibleRole = SessionRoleCount,
        ServerVersionRole
    };

    explicit QnDiscoveredSessionsModel(QObject *parent = nullptr);
    ~QnDiscoveredSessionsModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    const QnLoginSession *session(int row) const override;

private:
    void at_moduleFinder_moduleAddressFound(const QnModuleInformation &moduleInformation, const SocketAddress &address);
    void at_moduleFinder_moduleAddressLost(const QnModuleInformation &moduleInformation, const SocketAddress &address);
    void at_moduleFinder_moduleChanged(const QnModuleInformation &moduleInformation);

private:
    QnModuleFinder *m_moduleFinder;

    struct DiscoveredSession : QnLoginSession {
        DiscoveredSession() : QnLoginSession() {}
        QnSoftwareVersion serverVersion;
        QnUuid moduleId;
    };

    QList<DiscoveredSession> m_discoveredSessions;
};
