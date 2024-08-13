// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_models.h"

#include <QStringRef>

#include <nx/fusion/model_functions.h>
#include <nx/utils/cryptographic_hash.h>

#include "combined_id.h"

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkIdV1, (json), BookmarkIdV1_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkIdV3, (json), BookmarkIdV3_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkFilterV1, (json), BookmarkFilterV1_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkFilterV3, (json), BookmarkFilterV3_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkV1, (json), BookmarkV1_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkWithRuleV1, (json), BookmarkWithRuleV1_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkV3, (json), BookmarkV3_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkWithRuleV3, (json), BookmarkWithRuleV3_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkTagFilter, (json), BookmarkTagFilter_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkSharingSettings, (json), BookmarkSharingSettings_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BookmarkDescriptionRequest,
    (json), BookmarkDescriptionRequest_Fields)

nx::Uuid BookmarkIdV3::bookmarkIdFromCombined(const QString& id)
{
    return combined_id::localIdFromCombined(id);
}

nx::Uuid BookmarkIdV3::serverIdFromCombined(const QString& id)
{
    return combined_id::serverIdFromCombined(id, '.');
}

DeprecatedFieldNames* BookmarkFilterBase::getDeprecatedFieldNames()
{
    static DeprecatedFieldNames kDeprecatedFieldNames{
        {"_orderBy", "column"}, //< Up to v6.0.
    };
    return &kDeprecatedFieldNames;
}

void BookmarkIdV3::setIds(const nx::Uuid& bookmarkId, const nx::Uuid& serverId)
{
    id = combined_id::combineId(bookmarkId, serverId);
}

nx::Uuid BookmarkIdV3::bookmarkId() const
{
    return bookmarkIdFromCombined(id);
}

nx::Uuid BookmarkIdV3::serverId() const
{
    return serverIdFromCombined(id);
}

bool BookmarkV1::operator==(const BookmarkV1& other) const
{
    return static_cast<const BookmarkBase&>(*this) == static_cast<const BookmarkBase&>(other)
        && static_cast<const BookmarkIdV1&>(*this) == static_cast<const BookmarkIdV1&>(other)
        && serverId == other.serverId;
}

bool BookmarkV3::operator==(const BookmarkV3& other) const
{
    return (const BookmarkBase&) *this == (const BookmarkBase&) other
        && (const BookmarkIdV3&) *this == (const BookmarkIdV3&) other
        && *share == *other.share;
}

std::chrono::milliseconds BookmarkProtection::getSyncTime(const QString& protection)
{
    const auto p = combined_id::findIdSeparator(protection, ':');
    if (!p)
        return {};

    return std::chrono::milliseconds(QStringRef(&protection, 0, *p).toLongLong());
}

QString BookmarkProtection::getProtection(
    const QString& digest,
    std::chrono::milliseconds synchronizedMs)
{
    nx::utils::QnCryptographicHash hasher(nx::utils::QnCryptographicHash::Algorithm::Sha256);

    hasher.addData(digest.toUtf8());
    hasher.addData(QByteArray().setNum(synchronizedMs.count()));

    return QString::number(synchronizedMs.count()) + ":" + hasher.result().toHex();
}

QString BookmarkProtection::getDigest(
    const nx::Uuid& bookmarkId, const nx::Uuid& serverId, const QString& password)
{
    nx::utils::QnCryptographicHash hasher(nx::utils::QnCryptographicHash::Algorithm::Sha256);

    BookmarkIdV3 id;
    id.setIds(bookmarkId, serverId);
    hasher.addData(id.id.toUtf8());
    hasher.addData(password.toUtf8());

    return hasher.result().toHex();
}

} // namespace nx::vms::api
