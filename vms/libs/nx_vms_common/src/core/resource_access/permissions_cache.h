// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>

#include <common/common_globals.h>
#include <nx/core/core_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::core::access {

/**
 * Permissions cache is the component of QnResourceAccessManager class and despite its name it
 * doesn't implement any caching strategy itself. It's a matrix-alike permissions storage.
 * Designed to be fast, with low memory footprint.
 *
 * With cached mode, owner QnResourceAccessManager instance runs following caching strategy:
 * - Pre-calculate all permissions for combinations (Access Subject, Resource) on start-up;
 * - Keep storage dense, calculate all new permissions and store it for further fast access;
 * - Drop storage and recalculate permissions on major context changes;
 */
class NX_VMS_COMMON_API PermissionsCache
{
public:
    using OptionalPermissions = std::optional<Qn::Permissions>;
    using ResourceIdsWithPermissions = std::vector<std::pair<nx::Uuid, OptionalPermissions>>;

    /**
     * Accessor for stored resource permissions. Method has O(1) complexity. Storage access is
     * thread safe, while any other operation isn't. All methods are satisfy the requirements of
     * reentrancy.
     * @param  subjectId Not null Id, data validation is responsibility of caller.
     * @param  resourceId Not null Id, data validation is responsibility of caller.
     * @returns Resource Permissions for given Access Subject if such were set, std::nullopt
     *     otherwise.
     */
    OptionalPermissions permissions(const nx::Uuid& subjectId, const nx::Uuid& resourceId) const;

    /**
     * Stores resource permissions for given resource access subject. Currently, since
     * permissions stored in the cache, they are considered valid. Method has O(1) complexity,
     * unless storage grow occurs, however, timings difference is marginally low.
     * @returns True if stored value actually has been changed.
     */
    bool setPermissions(const nx::Uuid& subjectId, const nx::Uuid& resourceId,
        Qn::Permissions permissions);

    /**
     * Nullify resource permissions for given resource access subject, that doesn't mean that
     * resource or access subject is no longer exist, rather is a consequence that stored
     * value is no more valid and should be recalculated. Method has O(1) complexity.
     * @returns True if there was actual not null value stored, even if it was Qn::NoPermissions.
     */
    bool removePermissions(const nx::Uuid& subjectId, const nx::Uuid& resourceId);

    /**
     * Returns all stored resource permissions for access subject. If storage kept dense and
     * coherent with actual Resource and Access Subject sets, it will contain permissions for
     * all resources. Nevertheless defined it defined solely by owner implementation.
     * @param  subjectId User Role or User resource Id. Passing nonexistent access subject Id
     *     leads to nothing, null Id considered as invalid input data.
     * @returns Stored data as is for given subject as pairs Id/Permission for given Id, if such
     *     exists.
     */
    ResourceIdsWithPermissions permissionsForSubject(const nx::Uuid& subjectId) const;

    /**
     * Removes resource access subject and all associated resource permissions. If subject was
     * also stored as resource, i.e subject was user resource, it will be removed as resource
     * either. Subsequent call of the removeResource() method won't make any further
     * changes to the storage. Method has O(|Res|+|Subj|) complexity in the worst case, but
     * single call may be considered as instant operation regardless of storage size due
     * actually low overhead.
     * @param  subjectId User Role or User resource Id. Passing nonexistent access subject Id
     *     leads to nothing, null Id considered as invalid input data.
     */
    void removeSubject(const nx::Uuid& subjectId);

    /**
     * Removes permissions for given resource for any stored resource access subject. If the
     * resource to be deleted is user resource, therefore is also an resource access subject, it
     * will be completely removed as a subject too. Subsequent call of the removeSubject()
     * method won't make any further changes to the storage. Method has O(|Res|+|Subj|)
     * complexity in the worst case, but single call may be considered as instant operation
     * regardless of storage size due actually low overhead.
     * @param  resourceId Any Id. Passing nonexistent Resource Id leads to nothing, null Id
     *     considered as invalid input data.
     */
    void removeResource(const nx::Uuid& resourceId);

    /**
     * Clears storage to its blank/initial state.
     */
    void clear();

private:
    using StorageTableRow = std::vector<OptionalPermissions>;
    std::unordered_map<nx::Uuid, StorageTableRow> m_storage;
    std::vector<nx::Uuid> m_resourcesOrder;
    std::unordered_map<nx::Uuid, int> m_indexOfResource;
    std::deque<int> m_sparseColumns;
};

} // namespace nx::core::access
