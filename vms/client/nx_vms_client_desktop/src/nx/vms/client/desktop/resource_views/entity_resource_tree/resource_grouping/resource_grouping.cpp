// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_grouping.h"

#include <cmath>

#include <QtCore/QCoreApplication>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/desktop/system_context.h>

namespace {

QString::const_iterator findDelimiter(
    const QString::const_iterator begin,
    const QString::const_iterator end,
    const QString::size_type delimiterIndex = 0,
    QString::size_type* stoppedAtIndex = nullptr)
{
    using nx::vms::client::desktop::entity_resource_tree::resource_grouping::kSubIdDelimiter;

    if ((delimiterIndex == 0) && (*begin == kSubIdDelimiter))
    {
        if (stoppedAtIndex)
            *stoppedAtIndex = 0;
        return begin;
    }

    QString::const_iterator result = begin;
    for (int i = 0; i < delimiterIndex + 1; ++i)
    {
        result = std::find(std::next(result), end, kSubIdDelimiter);
        if (result == end)
        {
            if (stoppedAtIndex)
                *stoppedAtIndex = i - 1;
            return result;
        }
    }

    if (stoppedAtIndex)
        *stoppedAtIndex = delimiterIndex;

    return result;
}

} // namespace

namespace nx::vms::client::desktop {
namespace entity_resource_tree {
namespace resource_grouping {

class ResourceGroupingStrings
{
    Q_DECLARE_TR_FUNCTIONS(
        nx::vms::client::desktop::entity_resource_tree::resource_grouping::ResourceGroupingStrings)

public:
    static QString newGroupName(int groupSequenceNumber)
    {
        return groupSequenceNumber == 0
            ? tr("New Group")
            : tr("New Group %1").arg(groupSequenceNumber);
    }
};

QString getNewGroupSubId()
{
    const auto resourcePool = appContext()->currentSystemContext()->resourcePool();
    const auto cloudResourcePool =
        appContext()->cloudLayoutsManager()->systemContext()->resourcePool();

    QSet<QString> usedGroupIds;

    const auto extractGroupSubIds =
        [&usedGroupIds](const QnResourcePtr& resource)
        {
            const auto compositeGroupId = resourceCustomGroupId(resource);

            if (compositeGroupId.isEmpty())
                return;

            auto splitGroupId = compositeGroupId.split(kSubIdDelimiter, Qt::SkipEmptyParts);
            for (const auto& groupId: splitGroupId)
                usedGroupIds.insert(groupId.toLower()); //< Case insensitive.
        };

    const auto allCameras = resourcePool->getAllCameras(nx::Uuid(), /*ignoreDesktopCameras*/ true);
    const auto allLayouts = resourcePool->getResources<QnLayoutResource>();
    const auto allCloudLayouts = cloudResourcePool->getResources<QnLayoutResource>();

    for (const auto& camera: allCameras)
        extractGroupSubIds(camera);

    for (const auto& layout: allLayouts)
        extractGroupSubIds(layout);

    for (const auto& cloudLayout: allCloudLayouts)
        extractGroupSubIds(cloudLayout);

    for (int i = 0; true; ++i)
    {
        auto result = ResourceGroupingStrings::newGroupName(i);
        if (!usedGroupIds.contains(result.toLower())) //< Case insensitive.
            return result;
    }
}

QString resourceCustomGroupId(const QnResourcePtr& resource)
{
    if (resource.isNull())
    {
        NX_ASSERT(false, "Invalid parameter");
        return {};
    }

    return resource->getProperty(kCustomGroupIdPropertyKey).trimmed();
}

void setResourceCustomGroupId(const QnResourcePtr& resource, const QString& newCompositeId)
{
    if (resource.isNull() || resource->hasFlags(Qn::removed))
    {
        NX_ASSERT(false, "Invalid parameter");
        return;
    }

    resource->setProperty(kCustomGroupIdPropertyKey, newCompositeId);
}

bool isValidSubId(const QString& subId)
{
    return !subId.isEmpty()
        && !subId.front().isSpace()
        && !subId.back().isSpace()
        && !(subId.contains(kSubIdDelimiter))
        && (subId.size() <= kMaxSubIdLength);
}

QString fixupSubId(const QString& subId)
{
    auto result = subId.trimmed();
    result.remove(kSubIdDelimiter);
    result.truncate(kMaxSubIdLength);

    return result;
}

QString getResourceTreeDisplayText(const QString& compositeId)
{
    const auto idDimension = compositeIdDimension(compositeId);
    if (idDimension > 0)
        return extractSubId(compositeId, std::min(idDimension, kUserDefinedGroupingDepth) - 1);

    return QString();
}

int compositeIdDimension(const QString& compositeId)
{
    auto compositeIdView = QStringView(compositeId).trimmed();

    if (compositeIdView.isEmpty())
        return 0;

    const auto begin = std::cbegin(compositeIdView);
    const auto end = std::cend(compositeIdView);

    int result = 1;

    auto trailingDelimiter = findDelimiter(begin, end);
    if (trailingDelimiter == end)
        return result;

    auto leadingDelimiter = trailingDelimiter;
    while (trailingDelimiter != end)
    {
        leadingDelimiter = trailingDelimiter;
        trailingDelimiter = findDelimiter(std::next(leadingDelimiter), end);
        if (std::distance(leadingDelimiter, trailingDelimiter) > 1)
            ++result;
    }

    return result;
}

QString trimCompositeId(const QString& compositeId, int dimension)
{
    if (dimension < 0)
    {
        NX_ASSERT(false, "Invalid parameter");
        return compositeId;
    }

    if (compositeId.isEmpty() || dimension == 0)
        return QString();

    const auto begin = std::cbegin(compositeId);
    const auto end = std::cend(compositeId);

    const auto trailingDelimiterItr = findDelimiter(begin, end, dimension - 1);
    if (trailingDelimiterItr == end)
        return compositeId;

    return compositeId.left(std::distance(begin, trailingDelimiterItr));
}

void cutCompositeIdFront(QString& compositeId, int subIdCount)
{
    if (subIdCount < 0)
    {
        NX_ASSERT(false, "Invalid parameter");
        return;
    }

    if (subIdCount == 0 || compositeId.isEmpty())
        return;

    if (subIdCount >= kUserDefinedGroupingDepth)
    {
        compositeId.clear();
        return;
    }

    const auto begin = std::cbegin(compositeId);
    const auto end = std::cend(compositeId);

    const auto trailingDelimiterItr = findDelimiter(begin, end, subIdCount - 1);
    if (trailingDelimiterItr == end)
    {
        compositeId.clear();
        return;
    }

    compositeId.remove(0, std::distance(begin, std::next(trailingDelimiterItr)));
}

QString extractSubId(const QString& compositeId, int subIdOrder)
{
    if (subIdOrder < 0)
    {
        NX_ASSERT(false, "Invalid parameter");
        return QString();
    }

    const auto begin = std::cbegin(compositeId);
    const auto end = std::cend(compositeId);

    if (subIdOrder == 0)
    {
        qsizetype stoppedAt = 0;
        const auto trailingDelimiterItr = findDelimiter(begin, end, 0, &stoppedAt);
        if (trailingDelimiterItr == end)
            return compositeId;

        if (stoppedAt == 0)
            return compositeId.left(std::distance(begin, trailingDelimiterItr));
    }

    qsizetype stoppedAt = 0;
    auto leadingDelimiterItr = findDelimiter(begin, end, subIdOrder - 1, &stoppedAt);
    if (stoppedAt != (subIdOrder - 1))
    {
        NX_ASSERT(false, "Attempt to access sub ID with order over composite ID dimension");
        return QString();
    }

    const auto trailingDelimiterItr = findDelimiter(std::next(leadingDelimiterItr), end, 0);
    return compositeId.mid(
        std::distance(begin, leadingDelimiterItr) + 1,
        std::distance(leadingDelimiterItr, trailingDelimiterItr) - 1);
}

QString appendSubId(const QString& compositeId, const QString& subId)
{
    if (subId.isEmpty())
        return compositeId;

    if (compositeId.isEmpty())
        return subId;

    const auto begin = std::cbegin(compositeId);
    const auto end = std::cend(compositeId);

    auto lastDelimiterItr = findDelimiter(begin, end, kUserDefinedGroupingDepth - 1);
    if (lastDelimiterItr != end)
    {
        NX_ASSERT(false, "Attempt to create composite ID with dimension over limit");
        return compositeId;
    }

    QString result(compositeId.size() + subId.size() + 1, kSubIdDelimiter);

    auto resultItr = std::copy(begin, end, std::begin(result));
    std::copy(std::cbegin(subId), std::cend(subId), std::next(resultItr));

    return result;
}

QString insertSubId(const QString& compositeId, const QString& subId, int insertBefore)
{
    if (subId.isEmpty())
        return compositeId;

    const auto groupIdDimension = compositeIdDimension(compositeId);
    if (groupIdDimension >= kUserDefinedGroupingDepth)
    {
        NX_ASSERT(false, "Attempt to create composite ID with dimension over limit");
        return compositeId;
    }

    if (insertBefore > groupIdDimension)
    {
        NX_ASSERT(false, "Attempt to insert new sub ID before nonexistent sub ID");
        return compositeId;
    }

    if (compositeId.isEmpty())
        return subId;

    const auto begin = std::cbegin(compositeId);
    const auto end = std::cend(compositeId);

    QString result(compositeId.size() + subId.size() + 1, kSubIdDelimiter);

    if (insertBefore == 0)
    {
        auto resultItr = std::copy(std::cbegin(subId), std::cend(subId), std::begin(result));
        std::copy(begin, end, std::next(resultItr));
        return result;
    }

    if (insertBefore == groupIdDimension)
    {
        auto resultItr = std::copy(begin, end, std::begin(result));
        std::copy(std::cbegin(subId), std::cend(subId), std::next(resultItr));
        return result;
    }

    const auto delimiterItr = findDelimiter(begin, end, insertBefore - 1);

    auto resultItr = std::copy(begin, delimiterItr, std::begin(result));
    resultItr = std::copy(std::cbegin(subId), std::cend(subId), std::next(resultItr));
    std::copy(std::next(delimiterItr, 1), end, std::next(resultItr));

    return result;
}

QString appendCompositeId(const QString& baseCompositeId, const QString& compositeId)
{
    const auto baseIdDimension = compositeIdDimension(baseCompositeId);

    if (baseIdDimension == 0)
        return compositeId.trimmed();

    if ((baseIdDimension == kUserDefinedGroupingDepth)
        || (compositeIdDimension(compositeId) == 0))
    {
        return baseCompositeId;
    }

    return baseCompositeId + kSubIdDelimiter + trimCompositeId(
        compositeId, kUserDefinedGroupingDepth - baseIdDimension);
}

QString removeSubId(const QString& compositeId, int subIdOrder)
{
    if (subIdOrder < 0)
    {
        NX_ASSERT(false, "Invalid parameter");
        return compositeId;
    }

    const auto begin = std::cbegin(compositeId);
    const auto end = std::cend(compositeId);

    if (begin == end)
    {
        NX_ASSERT(false, "Attempt to remove sub ID with order over composite ID dimension");
        return compositeId;
    }

    if (subIdOrder == 0)
    {
        qsizetype stoppedAt = 0;
        const auto trailingDelimiterItr = findDelimiter(begin, end, 0, &stoppedAt);
        if (trailingDelimiterItr == end)
            return QString();

        return compositeId.right(std::distance(std::next(trailingDelimiterItr), end));
    }

    qsizetype stoppedAt = 0;
    auto leadingDelimiterItr = findDelimiter(begin, end, subIdOrder - 1, &stoppedAt);
    if (stoppedAt != (subIdOrder - 1))
    {
        NX_ASSERT(false, "Attempt to remove sub ID with order over composite ID dimension");
        return QString();
    }

    const auto trailingDelimiterItr = findDelimiter(std::next(leadingDelimiterItr), end, 0);

    if (trailingDelimiterItr == end)
        return compositeId.left(std::distance(begin, leadingDelimiterItr));

    const int resultSize =
        compositeId.size() - std::distance(leadingDelimiterItr, trailingDelimiterItr);
    QString result(resultSize, kSubIdDelimiter);

    auto resultItr = std::copy(begin, std::next(leadingDelimiterItr), std::begin(result));
    std::copy(std::next(trailingDelimiterItr), std::cend(compositeId), resultItr);

    return result;
}

QString replaceSubId(const QString& compositeId, const QString& subId, int subIdOrder)
{
    if (subId.isEmpty())
        return removeSubId(compositeId, subIdOrder);

    if (subIdOrder < 0)
    {
        NX_ASSERT(false, "Invalid parameter");
        return compositeId;
    }

    const auto begin = std::cbegin(compositeId);
    const auto end = std::cend(compositeId);

    if (begin == end)
    {
        NX_ASSERT(false, "Attempt to replace sub ID with order over composite ID dimension");
        return compositeId;
    }

    if (subIdOrder == 0)
    {
        qsizetype stoppedAt = 0;
        const auto trailingDelimiterItr = findDelimiter(begin, end, 0, &stoppedAt);
        if (trailingDelimiterItr == end)
            return subId;

        return subId + compositeId.right(std::distance(trailingDelimiterItr, end));
    }

    qsizetype stoppedAt = 0;
    auto leadingDelimiterItr = findDelimiter(begin, end, subIdOrder - 1, &stoppedAt);
    if (stoppedAt != (subIdOrder - 1))
    {
        NX_ASSERT(false, "Attempt to replace sub ID with order over composite ID dimension");
        return QString();
    }

    const auto trailingDelimiterItr = findDelimiter(std::next(leadingDelimiterItr), end, 0);

    if (trailingDelimiterItr == end)
        return compositeId.left(std::distance(begin, leadingDelimiterItr) + 1) + subId;

    const int resultSize = compositeId.size() + subId.size() -
        std::distance(leadingDelimiterItr, trailingDelimiterItr) + 1;

    QString result(resultSize, kSubIdDelimiter);

    auto resultItr = std::copy(begin, std::next(leadingDelimiterItr), std::begin(result));
    resultItr = std::copy(std::cbegin(subId), std::cend(subId), resultItr);
    std::copy(trailingDelimiterItr, end, resultItr);

    return result;
}

} // namespace resource_grouping
} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
