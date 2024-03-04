// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

#include "detail/resources_structures.h"

class QnWorkbenchContext;

namespace nx::vms::client::desktop::jsapi {

namespace detail { class ResourcesApiBackend; }

/**
 * Contains methods and signals to work with available resources.
 */
class Resources: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    Resources(QObject* parent = nullptr);

    virtual ~Resources() override;

    /**
     * @addtogroup vms-resources
     * Allows user to gather Resource descriptions.
     *
     * Contains methods and signals to work with available resources.
     *
     * Example:
     *
     *     const handleResourceAdded =
     *         function(resource)
     *         {
     *             if (resource.type = 'io_module')
     *                 vms.log.info(`Resource "${resource.name}" is IO module`)
     *
     *             if (vms.resources.hasMediaStream(resource.id))
     *                 vms.log.info(`Resource "${resource.name}" has media stream`)
     *         }
     *
     *     // You can connect to the signal using "connect" function and specifying a callback.
     *     vms.resources.added.connect(handleResourceAdded)
     *
     *     const resources = await vms.resources.resources()
     *     resources.forEach(handleResourceAdded)
     *
     * @{
     */

    /**
     * Checks if the Resource with the specified id can have a media stream.
     * @ingroup vms-resources
     */
    Q_INVOKABLE bool hasMediaStream(const QString& resourceId) const;

    /**
     * @return List of all available (to the user and by the API constraints) Resources.
     */
    Q_INVOKABLE QList<Resource> resources() const;

    /**
     * @return Description of the Resource with the specified identifier.
     */
    Q_INVOKABLE ResourceResult resource(const QString& resourceId) const;

signals:
    /**
     * Called when a new Resource is added.
     * @param resource Description of the Resource.
     */
    void added(const Resource& resource);

    /**
     * Called when a Resource is deleted.
     * @param resourceId Identifier of the Resource.
     */
    void removed(const QString& resourceId);

    /** @} */ // group vms.resources

private:
    nx::utils::ImplPtr<detail::ResourcesApiBackend> d;
};

} // namespace nx::vms::client::desktop::jsapi
