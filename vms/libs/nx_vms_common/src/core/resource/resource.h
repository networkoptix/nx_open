// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>

#include <common/common_globals.h>
#include <utils/common/from_this_to_shared.h>

#include "resource_fwd.h"
#include "resource_type.h"
#include "shared_resource_pointer.h"

class QnResourcePool;

namespace nx::vms::common { class SystemContext; }

namespace nx::vms::api {

struct ResourceParamData;
using ResourceParamDataList = std::vector<ResourceParamData>;

} // namespace nx::vms::api

/**
 * One of the base VMS classes, QnResource, represents a database-stored entity and provides the
 * interface to modify it or to listen to the changes.
 *
 * Most noticeable descendant classes are:
 * - QnVirtualCameraResource - Device entity;
 * - QnUserResource - VMS User;
 * - QnMediaServerResource - VMS Server;
 * - QnLayoutResource - Layout of cameras.
 *
 * Most db-stored entities are Resources, though there are some exceptions (e.g. User Roles or
 * Showreels). Some entities are linked to others using child-parent relations by the parentId
 * field, e.g. Storages and Devices belong to Servers, and Layouts can belong to users or Video
 * Walls.
 *
 * All work with QnResource-based classes is done using special shared pointers (see
 * QnSharedResourcePointer). There are typedefs for each pointer type, e.g. QnResourcePtr for
 * the base class, or QnUserResourcePtr for the User class.
 *
 * Available Resources are stored in a special QnResourcePool instance. The primary key of the
 * Resource is its id. All work with the Resources (and listening to their changes) should be done
 * only with the Resources belonging to the Resources pool. Any class storing a shared pointer to a
 * Resource must listen to the Resource Pool signals to correctly clean up the pointer when the
 * Resource is removed from the Resource Pool.
 *
 * Some of the Resource parameters and properties are stored in separate classes but are accessible
 * using the QnResource methods. Default values for some of the properties are stored not for the
 * Resource itself but for the Resource type (see getTypeId() and QnResourceType).
 *
 * No Resource property should be modified by both the Client and Server simultaneously. That's why
 * some properties are stored as fields and some are stored externally (for example, see
 * CameraAttributesData). The same field can be written by a Server for one Resource type and by
 * a Client for another (e.g. a camera name is modified by a Server, but a User or a Layout name -
 * by a Client).
 *
 * NOTE: All class methods are thread-safe.
 */
class NX_VMS_COMMON_API QnResource: public QObject, public QnFromThisToShared<QnResource>
{
    using ResourceStatus = nx::vms::api::ResourceStatus;
    Q_OBJECT

    Q_FLAGS(Qn::ResourceFlags)
    Q_PROPERTY(QnUuid id READ getId CONSTANT)
    Q_PROPERTY(QString name READ getName WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(Qn::ResourceFlags flags READ flags WRITE setFlags NOTIFY flagsChanged)
    Q_PROPERTY(nx::vms::api::ResourceStatus status READ getStatus WRITE setStatus
        NOTIFY statusChanged)

public:
    //---------------------------------------------------------------------------------------------
    // Constructing, copying and updating.

    // TODO: #sivanov Remove common module from the base Resource classes.
    /**
     * Creates a Resource belonging to some Common Module. The link is required mostly to access
     * various properties stored outside of the Resource.
     */
    QnResource();

    // Resource copying is prohibited as it creates an inconsistent entity: duplicate Resource,
    // which can contain link to Resource pool but does not belong to it.
    QnResource(const QnResource&) = delete;

    virtual ~QnResource() override;

    /**
     * Converts the Resource to its shared pointer. For this method to work, the very first
     * shared pointer must be created manually. See the QnFromThisToShared implementation.
     */
    template<class Resource>
    static QnSharedResourcePointer<Resource> toSharedPointer(const Resource* resource);

    /**
     * Converts the Resource to its shared pointer. For this method to work, the very first
     * shared pointer must be created manually. See the QnFromThisToShared implementation.
     */
    QnResourcePtr toSharedPointer() const;

    /**
     * Resource changes are implemented in the following way: when the new data is received (either
     * by a network request, or by discovering user changes on the camera), a new shared pointer to
     * the same Resource is created. Then it is sent to the Resource Pool - and if a Resource with
     * such  id is absent, it is added, otherwise the update() method is called for the existing
     * resource.
     *
     * All field data is assigned from the `source` Resource using the overloaded updateInternal()
     * virtual method. After changes are applied, the corresponding signals are sent. Important:
     * signals must never be sent when the Resource mutex is locked.
     */
    void update(const QnResourcePtr& source);

    /**
     * Resource Pool the Resource belongs to. Empty when the Resource is created. Filled when the
     * Resource is added to a Resource Pool. Corresponding signal is sent by the Resource Pool
     * then. Shared pointers to Resources outside of the Pool must not be stored anywhere and
     * should not be used except for temporary purposes.
     */
    QnResourcePool* resourcePool() const;

    /**
     * Assigns the Resource to the corresponding System Context. Normally this is done only by
     * the Resource Pool when the Resource is added.
     *
     * A Resource can never be removed from the System Context or moved to another one.
     */
    void addToSystemContext(nx::vms::common::SystemContext* systemContext);

    /**
     * Context of the System this Resource belongs to. Used to get access to external properties
     * of the Resource and to some tightly bound classes.
     */
    nx::vms::common::SystemContext* systemContext() const;

    /**
     * Debug helper for NX_LOG implementation. Used by toString(const T*).
     */
    virtual QString idForToStringFromPtr() const;

    //---------------------------------------------------------------------------------------------
    // Data fields.

    /**
     * Primary resource identifier. Must never be changed after the Resource is used, so it is not
     * guarded by a mutex.
     */
    const QnUuid& getId() const { return m_id; }

    /**
     * Updates the Resource id. The function is not protected by the mutex, as the existing
     * Resource id must never be changed after the Resource is added to a Resource Pool.
     */
    void setIdUnsafe(const QnUuid& id);

    /**
     * Id of the "parent" Resource. This relation can be used by different types of Resources with
     * different purposes. Main examples are:
     * - Device's parent is a Server, which currently governs it and records its footage;
     * - Storage's parent is a Server which uses it;
     * - Layout's parent can be a User who privately owns it, or a Video Wall Resource.
     */
    QnUuid getParentId() const;

    // TODO: #sivanov Make this method non-virtual, listen to the corresponding signal instead.
    virtual void setParentId(const QnUuid& parent);

    /**
     * Helper function to find the parent Resource in the same Resource Pool.
     */
    QnResourcePtr getParentResource() const;


    /**
     * Identifier of the Device type. Actually used only for Camera Resources - to access
     * Device read-only or default properties (e.g. the maximum fps value or sensor configuration).
     * Such parameters are stored in a separate json file, available online. The Server regularly
     * reads it, updates the internal values and provides them to the connecting Clients. See
     * QnResourceType.
     */
    QnUuid getTypeId() const;

    /**
     * Updates the Resource type id. Actual only for Cameras, when the same Camera is found by a
     * new more actual driver, or when a new Resource type is added to the Resource Type Pool.
     */
    void setTypeId(const QnUuid& id);

    /**
     * Availability of the Resource. Actual mostly for the network Resources: Devices, Servers, Web
     * Pages - to see if it is online or not. Contains some special values for Camera-specific or
     * Server-specific states.
     *
     * For local Resources, the status can also be used - e.g. to check if the file still exists.
     */
    virtual ResourceStatus getStatus() const;

    /**
     * The previous Resource status before the latest change. Usable only in a very limited scope,
     * mostly when we need to do some specific action when an online Camera becomes offline, or
     * vice versa.
     */
    ResourceStatus getPreviousStatus() const;

    /**
     * Updates Resource status. Behavior can differ, depending on the source of changes. The Server
     * can change Camera status based on its own data, or when some changes are received from
     * another network peer.
     */
    virtual void setStatus(
        ResourceStatus newStatus,
        Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local);

    /**
     * Helper function to check if the Camera status is "online" or "recording". Must not be used
     * anywhere else.
     */
    virtual bool isOnline() const { return isOnline(getStatus()); }

    /**
     * Helper function to check if the Camera status is "online" or "recording". Must not be used
     * anywhere else.
     */
    static bool isOnline(ResourceStatus status)
    {
        return status == ResourceStatus::online || status == ResourceStatus::recording;
    }

    /**
     * Flags are local-only bits of information about the Resource. They are used to quickly check
     * some feature support, or filter Resources by their type. One of the most important flags is
     * Qn::removed, which means this Resource has been removed from the Resource Pool and is going
     * to be deleted soon.
     */
    virtual Qn::ResourceFlags flags() const;

    /**
     * Resets Resource flags to the provided value.
     */
    void setFlags(Qn::ResourceFlags flags);

    /**
     * Checks if the Resource has the full set of the provided flags.
     */
    bool hasFlags(Qn::ResourceFlags flags) const { return (this->flags() & flags) == flags; }

    /**
     * Adds flags to the Resource.
     */
    void addFlags(Qn::ResourceFlags flags);

    /**
     * Removes flags from the Resource.
     */
    void removeFlags(Qn::ResourceFlags flags);

    /**
     * Resource name. For most of the Resource types, it can be modified by the Client directly,
     * but for Cameras and Servers it is modified by a Server. Due to the architecture limitation,
     * both the Client and the Server cannot modify the same field, so when it comes to the Camera
     * or Server name, we have a separate database field (Server-side value) and user attributes
     * field (Client-side value). An overridden virtual method provides the actual name depending
     * on the Resource class.
     */
    virtual QString getName() const;

    /**
     * Updates the Resource name. Depending on the Resource type, an overridden method will write
     * it either directly to the Resource field, or to a separate user attributes dictionary.
     */
    virtual void setName(const QString& name);

    /**
     * Resource url. Has a different meaning depending on the Resource type: network url for
     * Devices, Servers and Web Pages, filesystem path for Storages and local files.
     */
    virtual QString getUrl() const;
    virtual void setUrl(const QString& url);

    /**
     * User-defined id which primary usage purpose is to support different integrations. By using
     * this id, the user can easily switch one Camera with another, and all API requests continue
     * to work as usual.
     *
     * Supported for Cameras and Layouts only. Ids should be unique between the same types (not
     * enforced though), but the same logical id for a Camera and a Layout is perfectly OK as they
     * are used in different API calls.
     */
    virtual int logicalId() const;
    virtual void setLogicalId(int value);

    //---------------------------------------------------------------------------------------------
    // Properties.
    //
    // Note that these functions hide the property API inherited from QObject. This is intended,
    // as QObject API cannot be used with QnResource anyway because of the threading issues.

    /**
     * Custom mutable attribute of the Resource. Stored separately in another dictionary. All
     * properties are stored as strings. If no property value is stored for the current Resource,
     * the default value is read from the Resource type description.
     */
    virtual QString getProperty(const QString& key) const;

    /**
     * Full list of all properties explicitly set for the Resource.
     */
    nx::vms::api::ResourceParamDataList getProperties() const;

    /**
     * Set the property value. An empty string will clean the explicitly set value. Be careful, as
     * after that a default value from the Resource type will be used.
     *
     * Properties are stored in a separate dictionary. If the Resource for some reason has no id
     * set, the properties are stored locally instead.
     *
     * @return Whether the stored property value has been modified by this call. Return `true` if property is modified.
     */
    virtual bool setProperty(
        const QString& key,
        const QString& value,
        bool markDirty = true);

    /**
     * Save modified properties to the database (using a network transaction).
     * @return Whether the request was finished successfully.
     */
    virtual bool saveProperties();

    /**
     * Save modified properties to the database.
     */
    virtual void savePropertiesAsync();

    /**
     * Force using local properties for the Resource. Actual mostly for the unit tests and for the
     * temporary Resources (like Camera sub-channels).
     */
    void forceUsingLocalProperties();

signals:
    void statusChanged(const QnResourcePtr& resource, Qn::StatusChangeReason reason);
    void nameChanged(const QnResourcePtr& resource);
    void parentIdChanged(const QnResourcePtr& resource, const QnUuid& previousParentId);
    void flagsChanged(const QnResourcePtr& resource);
    void urlChanged(const QnResourcePtr& resource);
    void logicalIdChanged(const QnResourcePtr& resource);

    // TODO: #sivanov Remove this signal. Listen only to the actual signals in all listeners.
    void resourceChanged(const QnResourcePtr& resource);

    void propertyChanged(
        const QnResourcePtr& resource,
        const QString& key,
        const QString& prevValue,
        const QString& newValue);

    // TODO: #sivanov Remove these signals as they belong to QnMediaResource.
    void mediaDewarpingParamsChanged(const QnResourcePtr& resource);
    void videoLayoutChanged(const QnResourcePtr& resource);
    void rotationChanged();

protected:
    using Notifier = std::function<void(void)>;
    using NotifierList = QList<Notifier>;

    /**
     * Update the System Context. Intended to be overloaded in the descendant classes when some
     * logic should be called instantly after addition.
     */
    virtual void setSystemContext(nx::vms::common::SystemContext* systemContext);

    /**
     * Copies all data from the source Resource and adds the corresponding signal sending to the
     * list of notifiers.
     *
     * The method is called under a mutex (both Resources are locked), so the locks are not
     * required, and no signals must be sent.
     */
    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers);

    /**
     * For the big part of the properties, the descendant classes can have method aliases and
     * corresponding signals. This method should be overridden to correctly reset cached values and
     * send a corresponding semantical signal.
     */
    virtual void emitPropertyChanged(
        const QString& key,
        const QString& prevValue,
        const QString& newValue);

    /**
     * Update url value without mutex locking.
     * @return if value was actually changed.
     */
    bool setUrlUnsafe(const QString& value);

private:
    bool useLocalProperties() const;

protected:
    /** Recursive mutex that is used when accessing Resource fields. */
    mutable nx::Mutex m_mutex;

    /**
     * Identifier of the parent Resource. Use Resource Pool to retrieve the actual parent Resource.
     */
    QnUuid m_parentId;

    /** Name of this Resource. */
    QString m_name;

    /** Url of this Resource, if any. */
    QString m_url;

    /** Mutex that is to be used when accessing a set of all consumers. */
    mutable nx::Mutex m_consumersMtx;

private:
    /** Identifier of this Resource. */
    QnUuid m_id;

    /** Identifier of the type of this Resource. */
    QnUuid m_typeId;

    /** Flags of this Resource that determine its type. */
    Qn::ResourceFlags m_flags;

    std::map<QString, QString> m_locallySavedProperties;

    /** System Context this Resource belongs to. */
    std::atomic<nx::vms::common::SystemContext*> m_systemContext{};

    std::atomic<bool> m_forceUseLocalProperties{false};
    std::atomic<ResourceStatus> m_previousStatus = ResourceStatus::undefined;
};

template<class Resource>
QnSharedResourcePointer<Resource> toSharedPointer(const Resource* resource)
{
    if (!resource)
        return QnSharedResourcePointer<Resource>();
    return resource->toSharedPointer().template staticCast<Resource>();
}

template<class Resource>
QnSharedResourcePointer<Resource> QnResource::toSharedPointer(const Resource* resource)
{
    using ::toSharedPointer; //< Let ADL kick in.
    return toSharedPointer(resource);
}
