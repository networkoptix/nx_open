// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/vms/client/mobile/window_context_aware.h>

namespace nx::vms::client::core { class CloudAuthRequest; }

namespace nx::vms::client::mobile {

namespace detail { class QmlResultWatcher; }

/** Handles cloud authorization requested by the cross-site contexts. */
class CloudActionHandler: public QObject, public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    CloudActionHandler(WindowContext* context, QObject* parent = nullptr);
    virtual ~CloudActionHandler() override;

    /**
     * Opens the cloud login screen and finishes the request with the screen result. Requests
     * which arrive while the screen is open share it.
     */
    void handleCloudAuthRequest(core::CloudAuthRequest* request);

private:
    QPointer<detail::QmlResultWatcher> m_cloudAuthScreenResult;
};

} // namespace nx::vms::client::mobile
