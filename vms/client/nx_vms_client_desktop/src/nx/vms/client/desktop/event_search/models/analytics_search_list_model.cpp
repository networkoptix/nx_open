// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_search_list_model.h"

#include <common/common_globals.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/managed_camera_set.h>

#include "private/analytics_search_list_model_p.h"

namespace nx::vms::client::desktop {

AnalyticsSearchListModel::AnalyticsSearchListModel(WindowContext* context, QObject* parent):
    base_type(context, [this]() { return new Private(this); }, parent),
    d(qobject_cast<Private*>(base_type::d.data()))
{
    setLiveSupported(true);
}

QRectF AnalyticsSearchListModel::filterRect() const
{
    return d->filterRect();
}

void AnalyticsSearchListModel::setFilterRect(const QRectF& relativeRect)
{
    d->setFilterRect(relativeRect);
}

TextFilterSetup* AnalyticsSearchListModel::textFilter() const
{
    return d->textFilter.get();
}

QnUuid AnalyticsSearchListModel::selectedEngine() const
{
    return d->selectedEngine();
}

void AnalyticsSearchListModel::setSelectedEngine(const QnUuid& value)
{
    d->setSelectedEngine(value);
}

QStringList AnalyticsSearchListModel::selectedObjectTypes() const
{
    return d->selectedObjectTypes();
}

void AnalyticsSearchListModel::setSelectedObjectTypes(const QStringList& value)
{
    d->setSelectedObjectTypes(value);
}

const std::set<QString>& AnalyticsSearchListModel::relevantObjectTypes() const
{
    return d->relevantObjectTypes();
}

QStringList AnalyticsSearchListModel::attributeFilters() const
{
    return d->attributeFilters();
}

void AnalyticsSearchListModel::setAttributeFilters(const QStringList& value)
{
    d->setAttributeFilters(value);
}

QString AnalyticsSearchListModel::combinedTextFilter() const
{
    const auto freeText = NX_ASSERT(textFilter())
        ? textFilter()->text()
        : QString();

    const auto attributesText = attributeFilters().join(" ");
    if (attributesText.isEmpty())
        return freeText;

    return freeText.isEmpty()
        ? attributesText
        : QString("%1 %2").arg(attributesText, freeText);
}

bool AnalyticsSearchListModel::isConstrained() const
{
    return filterRect().isValid()
        || !d->textFilter->text().isEmpty()
        || !selectedEngine().isNull()
        || !selectedObjectTypes().isEmpty()
        || !attributeFilters().empty()
        || base_type::isConstrained();
}

AnalyticsSearchListModel::LiveProcessingMode AnalyticsSearchListModel::liveProcessingMode() const
{
    return d->liveProcessingMode();
}

void AnalyticsSearchListModel::setLiveProcessingMode(LiveProcessingMode value)
{
    d->setLiveProcessingMode(value);
}

int AnalyticsSearchListModel::availableNewTracks() const
{
    return d->availableNewTracks();
}

void AnalyticsSearchListModel::commitAvailableNewTracks()
{
    d->commitNewTracks();
}

void AnalyticsSearchListModel::setLiveTimestampGetter(LiveTimestampGetter value)
{
    d->setLiveTimestampGetter(value);
}

bool AnalyticsSearchListModel::hasAccessRights() const
{
    return system()->accessController()->isDeviceAccessRelevant(
        nx::vms::api::AccessRight::viewArchive);
}

bool AnalyticsSearchListModel::isFilterDegenerate() const
{
    return AbstractAsyncSearchListModel::isFilterDegenerate() || hasOnlyLiveCameras();
}

bool AnalyticsSearchListModel::hasOnlyLiveCameras() const
{
    const QnVirtualCameraResourceSet cameras = cameraSet()->cameras();
    return !cameras.empty() && std::none_of(cameras.begin(), cameras.end(),
        [this](const QnVirtualCameraResourcePtr& camera) { return d->canViewArchive(camera); });
}


} // namespace nx::vms::client::desktop
