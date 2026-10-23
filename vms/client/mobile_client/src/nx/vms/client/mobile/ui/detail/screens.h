// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <future>

#include <QtCore/QObject>

#include <nx/vms/client/mobile/window_context_aware.h>

namespace nx::network::http { class Credentials; }

namespace nx::vms::client::mobile {

class WindowContext;

namespace detail {

class QmlResultWatcher;

class Screens: public QObject, public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    Screens(WindowContext* context, QObject* parent = nullptr);

    /** Open 2FA validation screen and report token validation result asynchronously. */
    void open2faValidationScreen(
        const network::http::Credentials& credentials, std::promise<bool> result) const;

    /**
     * Open cloud authentication screen without waiting for the result. The result is "success"
     * when the new auth data is applied to CloudStatusWatcher, "rejected" when the user closes the
     * screen, and "failed" or empty otherwise. Returns null if the screen can't be opened.
     */
    QmlResultWatcher* openCloudLoginScreen(
        bool reauthentication, const QString& user = {}, const QString& token = {}) const;
};

} // namespace detail

} // namespace nx::vms::client::mobile
