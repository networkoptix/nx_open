// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <core/resource/resource_fwd.h>

class QnResourcePool;

namespace nx::vms::client::desktop {
namespace entity_resource_tree {
namespace resource_grouping {

inline constexpr int kUserDefinedGroupingDepth = 8;
inline constexpr int kMaxSubIdLength = 80;
inline constexpr QChar kSubIdDelimiter = QChar::LineFeed;
static const QString kCustomGroupIdPropertyKey("customGroupId");

/**
 * @returns Unique name (comparison is case insensitive) for the newly created group from the
 *     sequence 'New Group, New Group 1, ..., New Group n'.
 */
NX_VMS_CLIENT_DESKTOP_API QString getNewGroupSubId();

/**
 * @param cameraResource Valid pointer to the QnResourcePtr expected.
 * @returns Composite ID, i.e chain of strings separated by line feed symbol which describes chain
 *     of nested groups, the last of which should contain given resource.
 */
NX_VMS_CLIENT_DESKTOP_API QString resourceCustomGroupId(const QnResourcePtr& resource);

/**
 * @param cameraResource Valid pointer to the QnResourcePtr expected.
 * @param compositeId Valid-formed composite ID string that will be stored in the given resource.
 */
NX_VMS_CLIENT_DESKTOP_API void setResourceCustomGroupId(
    const QnResourcePtr& resource,
    const QString& compositeId);

/**
 * @returns True if passed sub ID string mets following conditions:
 *     - Not empty
 *     - No leading and trailing whitespace
 *     - Doesn't contain kSubIdDelimiter character
 *     - Length doesn't exceed kMaximumSubIdLenth
 */
NX_VMS_CLIENT_DESKTOP_API bool isValidSubId(const QString& subId);

/**
 * @param subId Sub ID string or, in other words, single group name inputted by user.
 * @returns Valid Sub ID, string made from the one passed as parameter if such transformation
 *     is possible, null QString otherwise.
 */
NX_VMS_CLIENT_DESKTOP_API QString fixupSubId(const QString& subId);

/**
 * @param compositeId Valid composite group ID in the canonical form expected.
 * @param subIdOrder Order of sub ID.
 * @returns String which will be used as display text.
 */
NX_VMS_CLIENT_DESKTOP_API QString getResourceTreeDisplayText(const QString& compositeId);

/**
 * @param compositeId Composite ID, the one in non-canonical form either allowed.
 * @returns Count of non-empty, printable substrings from given composite ID.
 */
NX_VMS_CLIENT_DESKTOP_API int compositeIdDimension(const QString& compositeId);

/**
 * @param Valid composite group ID in the canonical form expected.
 * @param dimension Any non-negative integer value which will be treated as desired dimension for
 *     the resulting composite ID.
 * @returns Composite ID obtained by removing sub IDs with order equal or higher than passed
 *     dimension. If there are no such sub IDs, returns the same composite ID as passed.
 */
NX_VMS_CLIENT_DESKTOP_API QString trimCompositeId(const QString& compositeId, int dimension);

/**
 * Removes up to <tt>subIdCount</tt> sub IDs from front of existing composite ID.
 * @param [in,out] compositeId composite group ID in the canonical form expected.
 * @param subIdCount Any non-negative integer. If exceeds actual composite ID dimension, there
 *     will be empty composite ID as result.
 */
NX_VMS_CLIENT_DESKTOP_API void cutCompositeIdFront(QString& compositeId, int subIdCount);

/**
 * @param Valid composite group ID in the canonical form expected.
 * @param subIdOrder Order, index in a simple words, of desired sub ID from the passed composite
 *     group ID.
 * @returns Non-null sub ID string, if passed order value is less than passed composite ID
 *     dimension, null QString otherwise.
 */
NX_VMS_CLIENT_DESKTOP_API QString extractSubId(const QString& compositeId, int subIdOrder);

/**
 * @param compositeId Valid composite group ID in the canonical form expected.
 * @param subId Valid non-null sub ID.
 * @returns Composite ID with greater dimension in case if resulting ID dimension not exceed
 *     dimension limit, same composite ID otherwise.
 */
NX_VMS_CLIENT_DESKTOP_API QString appendSubId(const QString& compositeId, const QString& subId);

/**
 * @param compositeId Valid composite group ID in the canonical form expected.
 * @param subId Valid non-null sub ID.
 * @returns Composite ID with greater dimension in case if resulting ID dimension not exceed
 *     dimension limit, same composite ID otherwise.
 */
NX_VMS_CLIENT_DESKTOP_API QString insertSubId(const QString& compositeId, const QString& subId,
    int insertBeforeIndex);

/**
 * @param baseCompositeId Valid composite group ID in the canonical form expected. Null one is
 *     acceptable either.
 * @param compositeId Valid composite group ID in the canonical form expected. Null one is
 *     acceptable either.
 * @returns Merged composite group ID string trimmed to not exceed declared dimension limit.
 */
NX_VMS_CLIENT_DESKTOP_API QString appendCompositeId(
    const QString& baseCompositeId,
    const QString& compositeId);

/**
 * @param compositeId Valid composite group ID in the canonical form expected.
 * @param subIdOrder Order of desired sub ID to remove.
 * @returns Composite id with lesser dimension in case if sub ID order is less than composite ID
 *     dimension, the same composite ID otherwise.
 */
NX_VMS_CLIENT_DESKTOP_API QString removeSubId(const QString& compositeId, int subIdOrder);

/**
 * @param compositeId Valid composite group ID in the canonical form expected.
 * @param subIdOrder Order of sub ID to replace. Value less than passed composite ID dimension
 *     expected.
 * @param subId New sub ID value.
 * @returns Same dimension composite ID as passed one, with sub ID with given order replaced.
 */
NX_VMS_CLIENT_DESKTOP_API QString replaceSubId(const QString& compositeId, const QString& subId,
    int subIdOrder);

} // namespace resource_grouping
} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
