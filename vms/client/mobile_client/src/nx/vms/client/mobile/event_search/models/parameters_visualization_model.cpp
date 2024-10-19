// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "parameters_visualization_model.h"

#include <QtQml/QtQml>

#include <core/resource/camera_resource.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/utils/math/math.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/event_search/utils/common_object_search_setup.h>
#include <nx/vms/client/core/event_search/utils/event_search_utils.h>
#include <nx/vms/client/core/system_context.h>

#include <nx/utils/scoped_model_operations.h>

namespace nx::vms::client::mobile {

using namespace nx::vms::client::core;

struct ParametersVisualizationModel::Private
{
    Q_DECLARE_TR_FUNCTIONS(ParametersVisualizationModel::Private)

public:
    struct Item
    {
        QString value;
        QString iconPath;

        bool operator==(const Item& other) const
        {
            return value == other.value
                && iconPath == other.iconPath;
        }
    };

    Private(ParametersVisualizationModel* q);
    void update();

    ParametersVisualizationModel* const q;
    nx::utils::ScopedConnections analyticsConnections;
    nx::utils::ScopedConnections connections;
    QPointer<AnalyticsSearchSetup> analyticsSetup = nullptr;
    QPointer<CommonObjectSearchSetup> setup = nullptr;

    using Items = std::vector<Item>;
    Items items;

private:
    void addTimeFilterValue(Items& result);
    void addCameraFilterValue(Items& result);
    void addPluginFilterValue(Items& result);
    void addObjectTypeFilterValue(Items& result);
    void addObjectAttributesFilterValue(Items& result);
};

ParametersVisualizationModel::Private::Private(ParametersVisualizationModel* q):
    q(q)
{
}

void ParametersVisualizationModel::Private::update()
{
    const auto newItems =
        [this]() -> Items
        {
            if (!setup)
                return {};

            Items result;
            addTimeFilterValue(result);
            addCameraFilterValue(result);
            addPluginFilterValue(result);
            addObjectTypeFilterValue(result);
            addObjectAttributesFilterValue(result);
            return result;
        }();

    if (items == newItems)
        return;

    const ParametersVisualizationModel::ScopedReset resetGuard(q);
    items = newItems;
}

void ParametersVisualizationModel::Private::addTimeFilterValue(Items& result)
{

    const auto timeValue =
        [this]() -> QString
        {
            return !setup || setup->timeSelection() == EventSearch::TimeSelection::anytime
                ? QString{}
                : EventSearchUtils::timeSelectionText(setup->timeSelection());
        }();

    if (!timeValue.isEmpty())
        result.push_back({timeValue});
}

void ParametersVisualizationModel::Private::addCameraFilterValue(Items& result)
{
    const auto cameraValue =
        [this]() -> QString
        {
            if (!setup || setup->cameraSelection() == EventSearch::CameraSelection::all)
                return {};

            return EventSearchUtils::cameraSelectionText(
                setup->cameraSelection(), setup->selectedCameras());
        }();

    if (!cameraValue.isEmpty())
        result.push_back({cameraValue});
}

void ParametersVisualizationModel::Private::addPluginFilterValue(Items& result)
{
    const auto pluginValue =
        [this]() -> QString
        {
            if (!analyticsSetup)
                return {};

            const auto selectedEngineId = analyticsSetup->engine().toString();
            if (selectedEngineId.isEmpty())
                return {};

            const auto taxonomy = q->systemContext()->taxonomyManager()->currentTaxonomy();
            const auto engine = taxonomy->engineById(selectedEngineId);
            return engine
                ? engine->name()
                : QString{};
        }();

    if (!pluginValue.isEmpty())
        result.push_back({pluginValue});
}

void ParametersVisualizationModel::Private::addObjectTypeFilterValue(Items& result)
{
    const auto objectTypesValue =
        [this]() -> Item
        {
            if (!analyticsSetup)
                return {};

            const auto& types = analyticsSetup->objectTypes();
            switch (types.size())
            {
                case 0:
                    return {};
                case 1:
                {
                    const auto taxonomy = q->systemContext()->taxonomyManager()->currentTaxonomy();
                    const auto objectType = taxonomy->objectTypeById(types.first());
                    return objectType
                        ? Item{objectType->name(), objectType->icon()}
                        : Item{};
                }
                default:
                    return Item{tr("%n Object Types", "%n is number of types", types.size()), ""};
            }
        }();

    if (!objectTypesValue.value.isEmpty())
        result.push_back(objectTypesValue);
}

void ParametersVisualizationModel::Private::addObjectAttributesFilterValue(Items& result)
{
    const auto objectAttributesValue =
        [this]() -> QString
        {
            if (!analyticsSetup)
                return {};

            const auto& attributes = analyticsSetup->attributeFilters();
            switch (attributes.size())
            {
                case 0:
                    return {};
                case 1:
                    return attributes.first();
                default:
                    return tr("%n Object Attributes", "%n is number of attributes",
                        attributes.size());
            }
        }();

    if (!objectAttributesValue.isEmpty())
        result.push_back({objectAttributesValue});
}

//-------------------------------------------------------------------------------------------------

void ParametersVisualizationModel::registerQmlType()
{
    qmlRegisterType<ParametersVisualizationModel>(
        "Nx.Mobile", 1, 0, "ParametersVisualizationModel");
}

ParametersVisualizationModel::ParametersVisualizationModel(QObject* parent):
    base_type(parent),
    core::SystemContextAware(core::SystemContext::fromQmlContext(this)),
    d(new Private(this))
{
}

ParametersVisualizationModel::~ParametersVisualizationModel()
{
}

AnalyticsSearchSetup* ParametersVisualizationModel::analyticsSearchSetup() const
{
    return d->analyticsSetup;
}

void ParametersVisualizationModel::setAnalyticsSearchSetup(AnalyticsSearchSetup* value)
{
    if (d->analyticsSetup == value)
        return;

    d->analyticsConnections = {};
    d->analyticsSetup = value;

    if (d->analyticsSetup)
    {
        const auto update = [this]() { d->update(); };
        d->analyticsConnections.add(connect(
            d->analyticsSetup, &AnalyticsSearchSetup::objectTypesChanged,this, update));
        d->analyticsConnections.add(connect(
            d->analyticsSetup, &AnalyticsSearchSetup::attributeFiltersChanged,this, update));
        d->analyticsConnections.add(connect(
            d->analyticsSetup, &AnalyticsSearchSetup::engineChanged, this, update));
    }
    emit analyticsSearchSetupChanged();
}

CommonObjectSearchSetup* ParametersVisualizationModel::searchSetup() const
{
    return d->setup;
}

void ParametersVisualizationModel::setSearchSetup(CommonObjectSearchSetup* value)
{
    if (d->setup == value)
        return;

    d->connections = {};
    d->setup = value;

    if (d->setup)
    {
        const auto update = [this]() { d->update(); };
        d->connections.add(connect(
            d->setup, &CommonObjectSearchSetup::timeSelectionChanged, this, update));
        d->connections.add(connect(
            d->setup, &CommonObjectSearchSetup::cameraSelectionChanged, this, update));
        d->connections.add(connect(
            d->setup, &CommonObjectSearchSetup::selectedCamerasChanged, this, update));
        d->connections.add(connect(
            d->setup, &CommonObjectSearchSetup::mixedDevicesChanged, this, update));
    }

    d->update();

    emit searchSetupChanged();
}

int ParametersVisualizationModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid()
        ? 0
        : d->items.size();
}

QVariant ParametersVisualizationModel::data(const QModelIndex& index, int role) const
{
    const int row = index.row();
    if (!qBetween(0, row, rowCount()))
        return {};

    const auto& item = d->items[row];
    switch (role)
    {
        case Qt::DisplayRole:
            return item.value;
        case Qt::DecorationRole:
            return item.iconPath;
        default:
            return {};
    }
}

} // namespace nx::vms::client::mobile
