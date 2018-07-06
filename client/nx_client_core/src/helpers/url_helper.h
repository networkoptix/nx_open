#pragma once

#include <QtCore/QUrl>
#include <nx/utils/url.h>

class QnUrlHelper
{
    Q_GADGET

public:
    QnUrlHelper(const nx::utils::Url& url = nx::utils::Url());

    Q_INVOKABLE nx::utils::Url url() const;

    Q_INVOKABLE QString scheme() const;
    Q_INVOKABLE QString host() const;
    Q_INVOKABLE int port() const;
    //! @returns host:port or just host if port is not present
    Q_INVOKABLE QString address() const;
    Q_INVOKABLE QString userName() const;
    Q_INVOKABLE QString password() const;
    Q_INVOKABLE QString path() const;

    Q_INVOKABLE nx::utils::Url cleanUrl() const;

private:
    nx::utils::Url m_url;
};

Q_DECLARE_METATYPE(QnUrlHelper)
