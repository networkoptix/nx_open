#pragma once

#include <client_core/connection_context_aware.h>

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>
#include <nx/utils/url.h>

class QnReconnectHelper: public QObject, public QnConnectionContextAware
{
    Q_OBJECT
public:
    QnReconnectHelper(QObject *parent = NULL);

    QnMediaServerResourceList servers() const;

    QnMediaServerResourcePtr currentServer() const;
    nx::utils::Url currentUrl() const;

    void markServerAsInvalid(const QnMediaServerResourcePtr &server);

    void next();

private:
    QnMediaServerResourceList m_servers;

    int m_currentIndex = -1;
    QString m_userName;
    QString m_password;
};
