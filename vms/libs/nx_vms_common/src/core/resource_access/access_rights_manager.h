// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/resource_access_map.h>
#include <nx/utils/impl_ptr.h>

#include "abstract_access_rights_manager.h"

namespace nx::core::access {

/**
 * A manager class to keep own resource access rights of each resource access subject.
 */
class NX_VMS_COMMON_API AccessRightsManager: public AbstractAccessRightsManager
{
    Q_OBJECT
    using base_type = AbstractAccessRightsManager;

public:
    explicit AccessRightsManager(QObject* parent = nullptr);
    virtual ~AccessRightsManager() override;

    // Querying access rights.

    /** Returns specified subject's own access rights map. */
    virtual ResourceAccessMap ownResourceAccessMap(const QnUuid& subjectId) const override;

    // Setting access rights.

    /** Reset all access rights. */
    void resetAccessRights(const QHash<QnUuid /*subjectId*/, ResourceAccessMap>& accessRights);

    /** Sets specified access rights for the specified subject. */
    void setOwnResourceAccessMap(const QnUuid& subjectId, const ResourceAccessMap& value);

    /** Removes access rights information for specified subjects. */
    void removeSubjects(const QSet<QnUuid>& subjectIds);

    /** Clears all access rights for all subjects. */
    void clear();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::core::access
