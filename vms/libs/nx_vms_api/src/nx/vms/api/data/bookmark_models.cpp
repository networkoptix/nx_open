// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_models.h"

#include <QStringRef>

#include <nx/fusion/model_functions.h>
#include <nx/utils/cryptographic_hash.h>

namespace nx::vms::api {

namespace {

std::optional<int> findIdSeparator(const QString& id, char separator)
{
    const auto p = id.indexOf(separator);
    if (p <= 0)
        return std::nullopt;

    return p;
}

} // namespace

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
    if (id.isEmpty())
        return {};

    const auto p = findIdSeparator(id, '_');
    if (!p)
        return nx::Uuid::fromStringSafe(id);

    return nx::Uuid::fromStringSafe(id.mid(0, *p));
}

nx::Uuid BookmarkIdV3::serverIdFromCombined(const QString& id)
{
    if (id.isEmpty())
        return {};

    const auto from = findIdSeparator(id, '_');
    if (!from)
        return {};

    const auto to = findIdSeparator(id, '.');
    return nx::Uuid::fromStringSafe(id.mid(*from + 1, to ? *to : -1));
}

void BookmarkIdV3::setIds(const nx::Uuid& bookmarkId, const nx::Uuid& serverId)
{
    id = bookmarkId.toSimpleString() + '_' + serverId.toSimpleString();
}

nx::Uuid BookmarkIdV3::bookmarkId() const
{
    return bookmarkIdFromCombined(id);
}

nx::Uuid BookmarkIdV3::serverId() const
{
    return serverIdFromCombined(id);
}

std::chrono::milliseconds BookmarkProtection::getSyncTime(const QString& protection)
{
    const auto p = findIdSeparator(protection, ':');
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

QString BookmarkProtection::getDigest(const QString& bookmarkId, const QString& password)
{
    nx::utils::QnCryptographicHash hasher(nx::utils::QnCryptographicHash::Algorithm::Sha256);

    hasher.addData(bookmarkId.toUtf8());
    hasher.addData(password.toUtf8());

    return hasher.result().toHex();
}

} // namespace nx::vms::api
