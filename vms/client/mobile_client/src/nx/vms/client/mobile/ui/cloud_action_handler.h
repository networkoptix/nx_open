// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <optional>

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/vms/client/mobile/window_context_aware.h>

namespace nx::vms::client::core {

class CloudAuthRequest;
enum class OauthDataRequestError;

} // namespace nx::vms::client::core

namespace nx::vms::client::mobile {

namespace detail { class QmlResultWatcher; }

/** Handles cloud re-authorization requested by the main session and the cross-site contexts. */
class CloudActionHandler: public QObject, public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    using ReauthorizationHandler =
        std::function<void(std::optional<core::OauthDataRequestError> error)>;

    CloudActionHandler(WindowContext* context, QObject* parent = nullptr);
    virtual ~CloudActionHandler() override;

    /**
     * Opens the cloud login screen and calls the handler once with the screen result. Requests
     * which arrive while the screen is open share it. The handler is called synchronously if the
     * screen can't be opened, and is not called if the context is destroyed earlier.
     */
    void requestCloudReauthorization(QObject* context, ReauthorizationHandler handler);

    /** Finishes the request with the result of requestCloudReauthorization(). */
    void handleCloudAuthRequest(core::CloudAuthRequest* request);

protected: // For testing.
    /** Takes the window context from the QML context on the first use. */
    explicit CloudActionHandler(QObject* parent = nullptr);

    virtual detail::QmlResultWatcher* openCloudLoginScreen();

private:
    QPointer<detail::QmlResultWatcher> m_cloudAuthScreenResult;
};

} // namespace nx::vms::client::mobile
