// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

namespace nx::vms::client::desktop {

/**
 * Provides authentication for Webpage requests.
 */
class AbstractWebAuthenticator: public QObject
{
    Q_OBJECT
public:
    AbstractWebAuthenticator() = default;
    virtual ~AbstractWebAuthenticator() = default;

    /** Request authentication for url. */
    virtual void requestAuth(const QUrl& url) = 0;

signals:
    /** Emitted with the authentication result. */
    void authReady(const QAuthenticator& credentials);
};

} // namespace nx::vms::client::desktop
