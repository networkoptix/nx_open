#ifndef QN_RECONNECT_HELPER_H
#define QN_RECONNECT_HELPER_H

#include <core/resource/resource_fwd.h>

#include <utils/common/uuid.h>

class QnReconnectHelper: public QObject {
    Q_OBJECT
public:
    QnReconnectHelper(QObject *parent = NULL);

    QnMediaServerResourcePtr currentServer() const;
    QUrl currentUrl() const;

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

#endif //QN_RECONNECT_HELPER_H