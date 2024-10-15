// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

/**
 * Actual implementation watches all Layouts, containing cross-system Cameras, and when connection
 * to the cloud system is established, corresponding layout items are removed and added again. This
 * looks like the simpliest way to force both resource tree and workbench layout to re-create item.
 *
 * For now all layouts in the current System Context are watched.
 * // TODO: @sivanov Switch to cloud-stored layouts.
 */
class CrossSystemLayoutsWatcher: public QObject
{
    Q_OBJECT

public:
    explicit CrossSystemLayoutsWatcher(QObject* parent = nullptr);
    ~CrossSystemLayoutsWatcher();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
