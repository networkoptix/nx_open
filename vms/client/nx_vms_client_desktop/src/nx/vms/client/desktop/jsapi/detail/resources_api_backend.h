// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>

#include "resources_structures.h"

class QnWorkbenchContext;

namespace nx::vms::client::desktop::jsapi::detail {

/** Class implements management functions for the resources. */
class ResourcesApiBackend: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    ResourcesApiBackend(QObject* parent = nullptr);

    virtual ~ResourcesApiBackend() override;

    /** List of all available (to user and by the API constraints) resources. */
    detail::Resource::List resources() const;

    /** Description of the resource with the specified identifier. */
    detail::ResourceResult resource(const ResourceUniqueId& resourceId) const;

signals:
    void added(const Resource& resource);
    void removed(const ResourceUniqueId& resourceId);
};

} // namespace nx::vms::client::desktop::jsapi::detail
