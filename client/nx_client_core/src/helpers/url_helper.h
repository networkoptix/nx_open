#pragma once

#include <QtCore/QUrl>

class QnUrlHelper
{
    Q_GADGET

public:
    QnUrlHelper(const QUrl& url = QUrl());

    static const QString cloudConnectionScheme;

    Q_INVOKABLE QUrl url() const;

    Q_INVOKABLE QString scheme() const;
    Q_INVOKABLE QString host() const;
    Q_INVOKABLE int port() const;
    //! @returns host:port or just host if port is not present
    Q_INVOKABLE QString address() const;
    Q_INVOKABLE QString userName() const;
    Q_INVOKABLE QString password() const;
    Q_INVOKABLE QString path() const;

    Q_INVOKABLE QUrl cleanUrl() const;

private:
    QUrl m_url;
};

Q_DECLARE_METATYPE(QnUrlHelper)
