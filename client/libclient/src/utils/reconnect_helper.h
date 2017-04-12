#pragma once

#include <client_core/connection_context_aware.h>

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

class QnReconnectHelper: public QObject, public QnConnectionContextAware
{
    Q_OBJECT
public:
    QnReconnectHelper(QObject *parent = NULL);

    QnMediaServerResourceList servers() const;

    QnMediaServerResourcePtr currentServer() const;
    QUrl currentUrl() const;

    void markServerAsInvalid(const QnMediaServerResourcePtr &server);

    void next();
private:
    struct InterfaceInfo {
        QUrl url;
        bool online;
        bool ignored;
        int count;

        InterfaceInfo();
    };

    void updateInterfacesForServer(const QnUuid &id);
    QUrl bestInterfaceForServer(const QnUuid &id);

    void addInterfaceIfNotExists(QList<InterfaceInfo> &interfaces, const InterfaceInfo &info) const;
    void replaceInterface(QList<InterfaceInfo> &interfaces, const InterfaceInfo &info) const;
private:
    QnMediaServerResourceList m_allServers;
    QnMediaServerResourcePtr m_currentServer;
    QUrl m_currentUrl;
    QString m_userName;
    QString m_password;
    QHash<QnUuid, QList<InterfaceInfo>> m_interfacesByServer;
};
