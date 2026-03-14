// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_cloud_status_watcher.h"

namespace nx::vms::client::core {

AbstractCloudStatusWatcher::AbstractCloudStatusWatcher(QObject* parent):
    QObject(parent)
{
}

AbstractCloudStatusWatcher::~AbstractCloudStatusWatcher()
{
}

} // namespace nx::vms::client::core
