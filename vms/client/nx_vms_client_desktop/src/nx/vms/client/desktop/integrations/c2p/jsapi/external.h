// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

class QnWorkbenchItem;

namespace nx::vms::client::desktop { class WindowContext; }

namespace nx::vms::client::desktop::integrations::c2p::jsapi {

/**
 * Compatibility JavaScript API for C2P.
 */
class External: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    External(QnWorkbenchItem* item, QObject* parent = nullptr);

    virtual ~External() override;

    Q_INVOKABLE void c2pplayback(const QString& cameraNames, int timestampSec);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::integrations::c2p::jsapi
