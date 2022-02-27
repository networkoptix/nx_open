// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class NX_VMS_CLIENT_DESKTOP_API AbstractResourceSource: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    AbstractResourceSource();
    virtual ~AbstractResourceSource();
    virtual QVector<QnResourcePtr> getResources() = 0;

signals:
    void resourceAdded(const QnResourcePtr& resource);
    void resourceRemoved(const QnResourcePtr& resource);
};

using AbstractResourceSourcePtr = std::unique_ptr<AbstractResourceSource>;

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
