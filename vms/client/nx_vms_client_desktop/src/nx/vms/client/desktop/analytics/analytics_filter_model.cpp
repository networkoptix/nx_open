// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_filter_model.h"

#include <QtQml/QQmlEngine>

#include <core/resource/camera_resource.h>
#include <nx/analytics/taxonomy/abstract_resource_support_proxy.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/utils/range_adapters.h>

#include "analytics_taxonomy_manager.h"
#include "taxonomy/abstract_state_view_filter.h"
#include "taxonomy/attribute_condition_state_view_filter.h"
#include "taxonomy/composite_state_view_filter.h"
#include "taxonomy/engine_state_view_filter.h"
#include "taxonomy/object_type.h"
#include "taxonomy/scope_state_view_filter.h"

namespace nx::vms::client::desktop::analytics::taxonomy {

namespace {

class LiveTypeFilter: public AbstractStateViewFilter
{
public:
    LiveTypeFilter(bool excludeLiveTypes, QObject* parent = nullptr);

    virtual QString id() const override;
    virtual QString name() const override { return ""; };

    virtual bool matches(
        const nx::analytics::taxonomy::AbstractObjectType* objectType) const override;

    virtual bool matches(
        const nx::analytics::taxonomy::AbstractAttribute* attribute) const override;

private:
    bool m_liveTypesExcluded = false;
};

LiveTypeFilter::LiveTypeFilter(bool excludeLiveTypes, QObject* parent):
    AbstractStateViewFilter(parent),
    m_liveTypesExcluded(excludeLiveTypes)
{
}

QString LiveTypeFilter::id() const
{
    return QString::number(static_cast<int>(m_liveTypesExcluded));
}

bool LiveTypeFilter::matches(const nx::analytics::taxonomy::AbstractObjectType* objectType) const
{
    return !(m_liveTypesExcluded && (objectType->isLiveOnly() || objectType->isNonIndexable()));
}

bool LiveTypeFilter::matches(const nx::analytics::taxonomy::AbstractAttribute* attribute) const
{
    return true;
}

void mapObjectTypes(
    const std::vector<taxonomy::ObjectType*>& objectTypes,
    QMap<QString, taxonomy::ObjectType*>& outObjectTypesById)
{
    if (objectTypes.empty())
        return;

    for (taxonomy::ObjectType* objectType: objectTypes)
    {
        outObjectTypesById.insert(objectType->id(), objectType);
        mapObjectTypes(objectType->derivedObjectTypes(), outObjectTypesById);
    }
}

QMap<QString, taxonomy::ObjectType*> objectTypesMap(
    const std::vector<taxonomy::ObjectType*>& objectTypes)
{
    QMap<QString, taxonomy::ObjectType*> result;
    mapObjectTypes(objectTypes, result);
    return result;
}

std::vector<nx::analytics::taxonomy::AbstractEngine*> enginesFromFilters(
    const std::vector<AbstractStateViewFilter*>& filters)
{
    std::vector<nx::analytics::taxonomy::AbstractEngine*> engines;
    for (auto filter: filters)
    {
        if (auto engineFilter = dynamic_cast<EngineStateViewFilter*>(filter))
            engines.push_back(engineFilter->engine());
    }
    return engines;
}

std::set<nx::Uuid> deviceIds(const QnVirtualCameraResourceSet& devices)
{
    std::set<nx::Uuid> ids;
    for (const QnVirtualCameraResourcePtr& device: devices)
        ids.insert(device->getId());

    return ids;
}

nx::common::metadata::Attributes filterAttributes(const QVariantMap& values)
{
    nx::common::metadata::Attributes result;
    for (const auto& [name, value]: nx::utils::constKeyValueRange(values))
        result.push_back({name, value.toString()});

    return result;
}

} // namespace

AnalyticsFilterModel::AnalyticsFilterModel(TaxonomyManager* taxonomyManager, QObject* parent):
    QObject(parent),
    m_taxonomyManager(taxonomyManager)
{
    if (taxonomyManager)
    {
        auto onTaxonomyChanged =
            [this]
            {
                if (m_isActive)
                    rebuild();

                const auto taxonomy = m_taxonomyManager->currentTaxonomy();

                if (!taxonomy->resourceSupportProxy())
                    return;

                m_manifestsUpdatedConnection.reset(connect(
                    taxonomy->resourceSupportProxy(),
                    &nx::analytics::taxonomy::AbstractResourceSupportProxy::manifestsMaybeUpdated,
                    this,
                    [this]
                    {
                        if (m_isActive)
                            rebuild();
                    }));
            };

        connect(taxonomyManager, &TaxonomyManager::currentTaxonomyChanged,
            this, onTaxonomyChanged);

        onTaxonomyChanged();
    }
}

std::vector<taxonomy::ObjectType*> AnalyticsFilterModel::objectTypes() const
{
    return m_objectTypes;
}

std::vector<nx::analytics::taxonomy::AbstractEngine*> AnalyticsFilterModel::engines() const
{
    return m_engines;
}

void AnalyticsFilterModel::setObjectTypes(const std::vector<taxonomy::ObjectType*>& objectTypes)
{
    m_objectTypes = objectTypes;
    m_objectTypesById = objectTypesMap(m_objectTypes);
    emit objectTypesChanged();
}

void AnalyticsFilterModel::setEngines(
    const std::vector<nx::analytics::taxonomy::AbstractEngine*>& engines)
{
    if (m_engines != engines)
    {
        m_engines = engines;
        emit enginesChanged();
    }
}

void AnalyticsFilterModel::update(
    nx::analytics::taxonomy::AbstractEngine* engine,
    const std::set<nx::Uuid>& devices,
    const QVariantMap& attributeValues,
    bool liveTypesExcluded,
    bool force)
{
    if (force
        || m_engine != engine
        || m_attributeValues != attributeValues
        || m_devices != devices
        || m_liveTypesExcluded != liveTypesExcluded)
    {
        m_engine = engine;
        m_devices = devices;
        m_attributeValues = attributeValues;
        m_liveTypesExcluded = liveTypesExcluded;

        const auto liveTypeFilter =
            new LiveTypeFilter(m_liveTypesExcluded, m_stateViewBuilder.get());

        const auto scopeFilter =
            new ScopeStateViewFilter(m_engine, m_devices, m_stateViewBuilder.get());

        const auto conditionFilter = new AttributeConditionStateViewFilter(
            /*baseFilter*/ nullptr, filterAttributes(m_attributeValues), m_stateViewBuilder.get());

        const auto filter = new CompositeFilter(
            std::vector<AbstractStateViewFilter*>{liveTypeFilter, scopeFilter, conditionFilter},
            m_stateViewBuilder.get());

        taxonomy::StateView* state = m_stateViewBuilder->stateView(filter);
        setObjectTypes(state->objectTypes());
    }
}

void AnalyticsFilterModel::setSelectedEngine(
    nx::analytics::taxonomy::AbstractEngine* engine,
    bool force)
{
    update(engine, m_devices, m_attributeValues, m_liveTypesExcluded);
}

void AnalyticsFilterModel::setSelectedDevices(const QnVirtualCameraResourceSet& devices)
{
    setSelectedDevices(deviceIds(devices));
}

void AnalyticsFilterModel::setSelectedDevices(const std::set<nx::Uuid>& devices)
{
    update(m_engine, devices, m_attributeValues, m_liveTypesExcluded);
}

void AnalyticsFilterModel::setSelectedAttributeValues(const QVariantMap& values)
{
    update(m_engine, m_devices, values, m_liveTypesExcluded);
}

void AnalyticsFilterModel::setLiveTypesExcluded(bool value)
{
    update(m_engine, m_devices, m_attributeValues, value);
}

ObjectType* AnalyticsFilterModel::objectTypeById(const QString& id) const
{
    auto objectType = m_objectTypesById[id];
    QQmlEngine::setObjectOwnership(objectType, QQmlEngine::CppOwnership);
    return objectType;
}

ObjectType* AnalyticsFilterModel::findFilterObjectType(const QStringList& analyticsObjectTypeIds)
{
    return objectTypeById(ObjectType::makeId(analyticsObjectTypeIds));
}

QStringList AnalyticsFilterModel::getAnalyticsObjectTypeIds(ObjectType* filterObjectType)
{
    return filterObjectType ? QStringList{filterObjectType->id()} : QStringList{};
}

bool AnalyticsFilterModel::isActive() const
{
    return m_isActive;
}

void AnalyticsFilterModel::setActive(bool value)
{
    if (m_isActive != value)
    {
        m_isActive = value;
        emit activeChanged();

        if (m_isActive)
            rebuild();
    }
}

void AnalyticsFilterModel::setSelected(
    nx::analytics::taxonomy::AbstractEngine* engine,
    const QVariantMap& attributeValues)
{
    update(engine, m_devices, attributeValues, m_liveTypesExcluded);
}

void AnalyticsFilterModel::rebuild()
{
    if (!m_taxonomyManager)
        return;

    setObjectTypes({});
    setEngines({});

    if (!NX_ASSERT(m_objectTypes.empty() && m_engines.empty()))
        return;

    m_stateViewBuilder =
        std::make_unique<taxonomy::StateViewBuilder>(m_taxonomyManager->currentTaxonomy());

    setEngines(enginesFromFilters(m_stateViewBuilder->engineFilters()));
    update(m_engine, m_devices, m_attributeValues, m_liveTypesExcluded, /*force*/ true);
}

} // namespace nx::vms::client::desktop::analytics::taxonomy
