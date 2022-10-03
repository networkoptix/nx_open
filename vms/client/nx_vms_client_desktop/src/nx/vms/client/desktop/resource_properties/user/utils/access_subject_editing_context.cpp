// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_subject_editing_context.h"

#include <memory>

#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/access_rights_resolver.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/vms/common/system_context.h>

#include "proxy_access_rights_manager.h"

namespace nx::vms::client::desktop {

using namespace nx::vms::api;
using namespace nx::vms::common;
using namespace nx::core::access;

// ------------------------------------------------------------------------------------------------
// AccessSubjectEditingContext::Private

class AccessSubjectEditingContext::Private: public QObject
{
    AccessSubjectEditingContext* const q;

public:
    class Hierarchy: public SubjectHierarchy
    {
    public:
        using SubjectHierarchy::SubjectHierarchy;
        using SubjectHierarchy::addOrUpdate;
        using SubjectHierarchy::remove;
        using SubjectHierarchy::operator=;
    };

public:
    QnUuid currentSubjectId;
    const QPointer<SubjectHierarchy> systemSubjectHierarchy;
    const std::unique_ptr<Hierarchy> currentHierarchy;
    const std::unique_ptr<ProxyAccessRightsManager> accessRightsManager;
    const std::unique_ptr<AccessRightsResolver> accessRightsResolver;
    const std::unique_ptr<AccessRightsResolver::Notifier> notifier;
    bool hierarchyChanged = false;

public:
    explicit Private(
        AccessSubjectEditingContext* q,
        nx::vms::common::SystemContext* systemContext)
        :
        q(q),
        systemSubjectHierarchy(systemContext->accessSubjectHierarchy()),
        currentHierarchy(new Hierarchy()),
        accessRightsManager(new ProxyAccessRightsManager(systemContext->accessRightsManager())),
        accessRightsResolver(new AccessRightsResolver(
            systemContext->resourcePool(),
            accessRightsManager.get(),
            systemContext->globalPermissionsWatcher(),
            currentHierarchy.get())),
        notifier(accessRightsResolver->createNotifier())
    {
        connect(notifier.get(), &AccessRightsResolver::Notifier::resourceAccessChanged,
            q, &AccessSubjectEditingContext::resourceAccessChanged);

        connect(accessRightsResolver.get(), &AccessRightsResolver::resourceAccessReset,
            q, &AccessSubjectEditingContext::resourceAccessChanged);

        connect(systemSubjectHierarchy, &SubjectHierarchy::reset,
            q, &AccessSubjectEditingContext::resetRelations);

        connect(systemSubjectHierarchy, &SubjectHierarchy::changed, this,
            [this](const QSet<QnUuid>& /*added*/, const QSet<QnUuid>& removed)
            {
                if (!hierarchyChanged)
                    this->q->resetRelations();
                else
                    currentHierarchy->remove(removed);

                if (removed.contains(currentSubjectId))
                {
                    this->q->setCurrentSubjectId({});
                    emit this->q->currentSubjectRemoved();
                }
            });

        connect(currentHierarchy.get(), &SubjectHierarchy::changed,
            q, &AccessSubjectEditingContext::hierarchyChanged);

        connect(currentHierarchy.get(), &SubjectHierarchy::reset,
            q, &AccessSubjectEditingContext::hierarchyChanged);
    }
};

// ------------------------------------------------------------------------------------------------
// AccessSubjectEditingContext

AccessSubjectEditingContext::AccessSubjectEditingContext(
    nx::vms::common::SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    d(new Private(this, systemContext))
{
}

AccessSubjectEditingContext::~AccessSubjectEditingContext()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnUuid AccessSubjectEditingContext::currentSubjectId() const
{
    return d->currentSubjectId;
}

void AccessSubjectEditingContext::setCurrentSubjectId(const QnUuid& value)
{
    if (d->currentSubjectId == value)
        return;

    NX_DEBUG(this, "Changing current subject to %1", value);

    d->currentSubjectId = value;
    d->accessRightsManager->setCurrentSubjectId(value);
    d->notifier->setSubjectId(value);
    resetRelations();

    emit subjectChanged();
}

void AccessSubjectEditingContext::setOwnResourceAccessMap(
    const ResourceAccessMap& resourceAccessMap)
{
    NX_DEBUG(this, "Setting access map to %1", resourceAccessMap);
    d->accessRightsManager->setOwnResourceAccessMap(resourceAccessMap);
}

void AccessSubjectEditingContext::resetOwnResourceAccessMap()
{
    NX_DEBUG(this, "Reverting access map changes");
    d->accessRightsManager->resetOwnResourceAccessMap();
}

ResourceAccessDetails AccessSubjectEditingContext::accessDetails(
    const QnResourcePtr& resource, AccessRight accessRight) const
{
    if (NX_ASSERT(!d->currentSubjectId.isNull()))
        return d->accessRightsResolver->accessDetails(d->currentSubjectId, resource, accessRight);

    return {};
}

void AccessSubjectEditingContext::setRelations(
    const QSet<QnUuid>& parents, const QSet<QnUuid>& members)
{
    if (!NX_ASSERT(!d->currentSubjectId.isNull()))
        return;

    NX_DEBUG(this, "Setting subject relations, parents: %1, members: %2", parents, members);

    if (!d->currentHierarchy->addOrUpdate(d->currentSubjectId, parents, members))
        return;

    d->hierarchyChanged = true;
    emit hierarchyChanged();
}

void AccessSubjectEditingContext::resetRelations()
{
    NX_DEBUG(this, "Reverting subject relations");

    if (NX_ASSERT(d->systemSubjectHierarchy))
        *d->currentHierarchy = *d->systemSubjectHierarchy;
    else
        d->currentHierarchy->clear();

    d->hierarchyChanged = false;
}

const SubjectHierarchy* AccessSubjectEditingContext::subjectHierarchy() const
{
    return d->currentHierarchy.get();
}

bool AccessSubjectEditingContext::hasChanges() const
{
    return d->hierarchyChanged || d->accessRightsManager->hasChanges();
}

void AccessSubjectEditingContext::revert()
{
    resetOwnResourceAccessMap();
    resetRelations();
}

} // namespace nx::vms::client::desktop
