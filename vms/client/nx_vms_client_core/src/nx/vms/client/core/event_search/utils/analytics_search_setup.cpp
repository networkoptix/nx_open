// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_search_setup.h"

#include <QtCore/QPointer>
#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/utils/managed_camera_set.h>

namespace nx::vms::client::core {

struct AnalyticsSearchSetup::Private
{
    const QPointer<AnalyticsSearchListModel> model;
    bool areaEnabled = false;
    bool areaSelectionActive = false;
};

void AnalyticsSearchSetup::registerQmlType()
{
    qmlRegisterUncreatableType<core::AnalyticsSearchSetup>("nx.vms.client.core", 1, 0,
        "AnalyticsSearchSetup", "Cannot create instance of AnalyticsSearchSetup");
}

AnalyticsSearchSetup::AnalyticsSearchSetup(
    AnalyticsSearchListModel* model,
    QObject* parent)
    :
    QObject(parent),
    d(new Private{model})
{
    if (!NX_ASSERT(model))
        return;

    connect(model, &AnalyticsSearchListModel::filterRectChanged,
        this, &AnalyticsSearchSetup::areaChanged);

    connect(model, &AnalyticsSearchListModel::selectedObjectTypesChanged,
        this, &AnalyticsSearchSetup::objectTypesChanged);

    connect(model, &AnalyticsSearchListModel::relevantObjectTypesChanged,
        this, &AnalyticsSearchSetup::relevantObjectTypesChanged);

    connect(model, &AnalyticsSearchListModel::availableNewTracksChanged,
        this, &AnalyticsSearchSetup::availableNewTracksChanged);

    connect(model, &AnalyticsSearchListModel::attributeFiltersChanged,
        this, &AnalyticsSearchSetup::attributeFiltersChanged);

    connect(model, &AnalyticsSearchListModel::combinedTextFilterChanged,
        this, &AnalyticsSearchSetup::combinedTextFilterChanged);

    connect(model, &AnalyticsSearchListModel::selectedEngineChanged,
        this, &AnalyticsSearchSetup::engineChanged);

    connect(model, &core::AbstractSearchListModel::camerasChanged, this,
        [this]()
        {
            const bool areaEnabled =
                d->model->cameraSet().type() == core::ManagedCameraSet::Type::single;

            if (areaEnabled == d->areaEnabled)
                return;

            if (!areaEnabled)
                clearAreaSelection();

            d->areaEnabled = areaEnabled;
            emit areaEnabledChanged();
        });
}

AnalyticsSearchSetup::~AnalyticsSearchSetup()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnUuid AnalyticsSearchSetup::engine() const
{
    return d->model ? d->model->selectedEngine() : QnUuid();
}

void AnalyticsSearchSetup::setEngine(const QnUuid& value)
{
    if (NX_ASSERT(d->model))
        d->model->setSelectedEngine(value);
}

QStringList AnalyticsSearchSetup::objectTypes() const
{
    return d->model ? d->model->selectedObjectTypes() : QStringList();
}

const std::set<QString>& AnalyticsSearchSetup::relevantObjectTypes() const
{
    static const std::set<QString> dummy;
    return d->model ? d->model->relevantObjectTypes() : dummy;
}

void AnalyticsSearchSetup::setObjectTypes(const QStringList& value)
{
    if (NX_ASSERT(d->model))
        d->model->setSelectedObjectTypes(value);
}

QStringList AnalyticsSearchSetup::attributeFilters() const
{
    return d->model ? d->model->attributeFilters() : QStringList();
}

void AnalyticsSearchSetup::setAttributeFilters(const QStringList& value)
{
    if (NX_ASSERT(d->model))
        d->model->setAttributeFilters(value);
}

QString AnalyticsSearchSetup::combinedTextFilter() const
{
    return d->model ? d->model->combinedTextFilter() : QString();
}

QRectF AnalyticsSearchSetup::area() const
{
    return d->model ? d->model->filterRect() : QRectF();
}

void AnalyticsSearchSetup::setArea(const QRectF& value)
{
    if (NX_ASSERT(d->model))
        d->model->setFilterRect(value);
}

bool AnalyticsSearchSetup::isAreaSelected() const
{
    return area().isValid();
}

bool AnalyticsSearchSetup::areaEnabled() const
{
    return d->areaEnabled;
}

bool AnalyticsSearchSetup::areaSelectionActive() const
{
    return d->areaSelectionActive;
}

void AnalyticsSearchSetup::setAreaSelectionActive(bool value)
{
    if (d->areaSelectionActive == value)
        return;

    d->areaSelectionActive = value;
    emit areaSelectionActiveChanged();
}

void AnalyticsSearchSetup::clearAreaSelection()
{
    setAreaSelectionActive(false);
    setArea({});
}

void AnalyticsSearchSetup::setLiveTimestampGetter(
    AnalyticsSearchListModel::LiveTimestampGetter value)
{
    if (NX_ASSERT(d->model))
        d->model->setLiveTimestampGetter(value);
}

AnalyticsSearchListModel* AnalyticsSearchSetup::model() const
{
    return d->model;
}

int AnalyticsSearchSetup::availableNewTracks() const
{
    return d->model ? d->model->availableNewTracks() : 0;
}

void AnalyticsSearchSetup::commitAvailableNewTracks()
{
    if (d->model)
        d->model->commitAvailableNewTracks();
}

} // namespace nx::vms::client::core
