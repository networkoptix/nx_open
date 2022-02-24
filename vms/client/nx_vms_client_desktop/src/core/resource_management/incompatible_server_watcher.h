// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>

namespace nx::vms::api {

struct DiscoveredServerData;
using DiscoveredServerDataList = std::vector<DiscoveredServerData>;

} // namespace nx::vms::api

class QnIncompatibleServerWatcherPrivate;

class QnIncompatibleServerWatcher:
    public QObject,
    public nx::vms::client::core::CommonModuleAware
{
    Q_OBJECT

public:
    explicit QnIncompatibleServerWatcher(QObject* parent = nullptr);
    virtual ~QnIncompatibleServerWatcher() override;

    void start();
    void stop();

    /*! Prevent incompatible resource from removal if it goes offline.
     * \param id Real server GUID.
     * \param keep true to keep the resource (if present), false to not keep.
     * In case when the resource was removed when was having 'keep' flag on
     * it will be removed in this method.
     */
    void keepServer(const QnUuid& id, bool keep);

private:
    Q_DECLARE_PRIVATE(QnIncompatibleServerWatcher)
    QScopedPointer<QnIncompatibleServerWatcherPrivate> d_ptr;
};
