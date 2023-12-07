// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_rights_resolver.h"

#include <memory>

#include <QtCore/QPointer>

#include "abstract_access_rights_manager.h"
#include "resolvers/inherited_resource_access_resolver.h"
#include "resolvers/intercom_layout_access_resolver.h"
#include "resolvers/layout_item_access_resolver.h"
#include "resolvers/own_resource_access_resolver.h"
#include "resolvers/videowall_item_access_resolver.h"

namespace nx::core::access {

// ------------------------------------------------------------------------------------------------
// AccessRightsResolver

struct AccessRightsResolver::Private
{
    std::unique_ptr<OwnResourceAccessResolver> ownResourceAccessResolver;
    std::unique_ptr<VideowallItemAccessResolver> videowallItemAccessResolver;
    std::unique_ptr<LayoutItemAccessResolver> layoutItemAccessResolver;
    std::unique_ptr<IntercomLayoutAccessResolver> intercomLayoutAccessResolver;
    std::unique_ptr<InheritedResourceAccessResolver> inheritedResourceAccessResolver;

    explicit Private(
        QnResourcePool* resourcePool,
        AbstractAccessRightsManager* accessRightsManager,
        AbstractGlobalPermissionsWatcher* globalPermissionsWatcher,
        SubjectHierarchy* subjectHierarchy,
        bool subjectEditingMode)
        :
        ownResourceAccessResolver(new OwnResourceAccessResolver(
            accessRightsManager, globalPermissionsWatcher)),
        videowallItemAccessResolver(new VideowallItemAccessResolver(
            ownResourceAccessResolver.get(), resourcePool)),
        layoutItemAccessResolver(new LayoutItemAccessResolver(
            videowallItemAccessResolver.get(), resourcePool, subjectEditingMode)),
        intercomLayoutAccessResolver(new IntercomLayoutAccessResolver(
            layoutItemAccessResolver.get(), resourcePool)),
        inheritedResourceAccessResolver(new InheritedResourceAccessResolver(
            intercomLayoutAccessResolver.get(), subjectHierarchy))
    {
    }
};

AccessRightsResolver::AccessRightsResolver(
    QnResourcePool* resourcePool,
    AbstractAccessRightsManager* accessRightsManager,
    AbstractGlobalPermissionsWatcher* globalPermissionsWatcher,
    SubjectHierarchy* subjectHierarchy,
    Mode mode,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(
        resourcePool,
        accessRightsManager,
        globalPermissionsWatcher,
        subjectHierarchy,
        mode == Mode::editing))
{
    using InternalNotifier = AbstractResourceAccessResolver::Notifier;
    const auto internalNotifier = d->inheritedResourceAccessResolver->notifier();

    connect(internalNotifier, &InternalNotifier::resourceAccessReset, this,
        [this]() { emit resourceAccessReset(QPrivateSignal()); }, Qt::DirectConnection);
}

AccessRightsResolver::~AccessRightsResolver()
{
    // Required here for forward-declared scoped pointer destruction.
}

nx::vms::api::AccessRights AccessRightsResolver::accessRights(const QnUuid& subjectId,
    const QnResourcePtr& resource) const
{
    return d->inheritedResourceAccessResolver->accessRights(subjectId, resource);
}

nx::vms::api::AccessRights AccessRightsResolver::accessRights(const QnUuid& subjectId,
    const QnUuid& resourceGroupId) const
{
    if (!NX_ASSERT(nx::vms::api::specialResourceGroup(resourceGroupId)))
        return {};

    return d->inheritedResourceAccessResolver->resourceAccessMap(subjectId).value(resourceGroupId);
}

ResourceAccessMap AccessRightsResolver::resourceAccessMap(const QnUuid& subjectId) const
{
    return d->inheritedResourceAccessResolver->resourceAccessMap(subjectId);
}

nx::vms::api::GlobalPermissions AccessRightsResolver::globalPermissions(
    const QnUuid& subjectId) const
{
    return d->inheritedResourceAccessResolver->globalPermissions(subjectId);
}

nx::vms::api::AccessRights AccessRightsResolver::availableAccessRights(
    const QnUuid& subjectId) const
{
    return d->inheritedResourceAccessResolver->availableAccessRights(subjectId);
}

bool AccessRightsResolver::hasFullAccessRights(const QnUuid& subjectId) const
{
    return d->inheritedResourceAccessResolver->hasFullAccessRights(subjectId);
}

ResourceAccessDetails AccessRightsResolver::accessDetails(
    const QnUuid& subjectId,
    const QnResourcePtr& resource,
    nx::vms::api::AccessRight accessRight) const
{
    return d->inheritedResourceAccessResolver->accessDetails(subjectId, resource, accessRight);
}

AccessRightsResolver::Notifier* AccessRightsResolver::createNotifier(QObject* parent)
{
    return new AccessRightsResolver::Notifier(this, parent);
}

// ------------------------------------------------------------------------------------------------
// AccessRightsResolver::Notifier

struct AccessRightsResolver::Notifier::Private
{
    QPointer<AccessRightsResolver> source;
    QPointer<AbstractResourceAccessResolver::Notifier> notifier;
    QnUuid subjectId;

    void subscribe()
    {
        if (notifier && !subjectId.isNull())
            notifier->subscribeSubjects({subjectId});
    }

    void unsubscribe()
    {
        if (notifier && !subjectId.isNull())
            notifier->releaseSubjects({subjectId});
    }
};

AccessRightsResolver::Notifier::Notifier(QObject* parent):
    QObject(parent),
    d(new Private{})
{
}

AccessRightsResolver::Notifier::Notifier(AccessRightsResolver* source, QObject* parent):
    Notifier(parent)
{
    setSource(source);
}

AccessRightsResolver::Notifier::~Notifier()
{
    // Required here for forward-declared scoped pointer destruction.
}

AccessRightsResolver* AccessRightsResolver::Notifier::source() const
{
    return d->source;
}

void AccessRightsResolver::Notifier::setSource(AccessRightsResolver* value)
{
    if (d->source == value)
        return;

    if (d->notifier)
        d->notifier->disconnect(this);

    d->unsubscribe();
    d->source = value;
    d->notifier = d->source->d->inheritedResourceAccessResolver->notifier();
    d->subscribe();

    if (d->notifier)
    {
        connect(d->notifier, &AbstractResourceAccessResolver::Notifier::resourceAccessChanged, this,
            [this](const QSet<QnUuid>& changedSubjectIds)
            {
                if (changedSubjectIds.contains(d->subjectId))
                    emit resourceAccessChanged(d->subjectId, QPrivateSignal());
            }, Qt::DirectConnection);
    }

    emit sourceChanged();
}

QnUuid AccessRightsResolver::Notifier::subjectId() const
{
    return d->subjectId;
}

void AccessRightsResolver::Notifier::setSubjectId(const QnUuid& value)
{
    if (d->subjectId == value)
        return;

    d->unsubscribe();
    d->subjectId = value;
    d->subscribe();

    emit subjectIdChanged();
}

} // namespace nx::core::access
