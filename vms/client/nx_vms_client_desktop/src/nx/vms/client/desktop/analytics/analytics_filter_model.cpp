// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_filter_model.h"

#include <QtQml/QQmlEngine>

#include <core/resource/camera_resource.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/client/desktop/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/desktop/analytics/taxonomy/abstract_state_view_filter.h>
#include <nx/vms/client/desktop/analytics/taxonomy/attribute_condition_state_view_filter.h>
#include <nx/vms/client/desktop/analytics/taxonomy/composite_state_view_filter.h>
#include <nx/vms/client/desktop/analytics/taxonomy/engine_state_view_filter.h>
#include <nx/vms/client/desktop/analytics/taxonomy/node.h>
#include <nx/vms/client/desktop/analytics/taxonomy/scope_state_view_filter.h>

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
    const std::vector<taxonomy::AbstractNode*>& objectTypes,
    QMap<QString, taxonomy::AbstractNode*>& outObjectTypesById)
{
    if (objectTypes.empty())
        return;

    for (taxonomy::AbstractNode* objectType: objectTypes)
    {
        outObjectTypesById.insert(objectType->id(), objectType);
        mapObjectTypes(objectType->derivedNodes(), outObjectTypesById);
    }
}

QMap<QString, taxonomy::AbstractNode*> objectTypesMap(
    const std::vector<taxonomy::AbstractNode*>& objectTypes)
{
    QMap<QString, taxonomy::AbstractNode*> result;
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

std::set<QnUuid> deviceIds(const QnVirtualCameraResourceSet& devices)
{
    std::set<QnUuid> ids;
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
    if (!NX_ASSERT(taxonomyManager))
        return;

    connect(taxonomyManager, &TaxonomyManager::currentTaxonomyChanged,
        this,
        [this]
        {
            if (m_isActive)
                rebuild();
        });

    rebuild();
}

std::vector<taxonomy::AbstractNode*> AnalyticsFilterModel::objectTypes() const
{
    return m_objectTypes;
}

std::vector<nx::analytics::taxonomy::AbstractEngine*> AnalyticsFilterModel::engines() const
{
    return m_engines;
}

void AnalyticsFilterModel::setObjectTypes(const std::vector<taxonomy::AbstractNode*>& objectTypes)
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
    const std::set<QnUuid>& devices,
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

        const auto liveTypeFilter = std::make_unique<LiveTypeFilter>(m_liveTypesExcluded);
        const auto scopeFilter = std::make_unique<ScopeStateViewFilter>(m_engine, m_devices);
        const auto conditionFilter = std::make_unique<AttributeConditionStateViewFilter>(
            /*baseFilter*/ nullptr, filterAttributes(m_attributeValues));

        const auto filter =
            std::make_unique<CompositeFilter>(std::vector<AbstractStateViewFilter*>{
                liveTypeFilter.get(), scopeFilter.get(), conditionFilter.get()});

        taxonomy::AbstractStateView* state = m_stateViewBuilder->stateView(filter.get());
        setObjectTypes(state->rootNodes());
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

void AnalyticsFilterModel::setSelectedDevices(const std::set<QnUuid>& devices)
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

AbstractNode* AnalyticsFilterModel::objectTypeById(const QString& id) const
{
    auto objectType = m_objectTypesById[id];
    QQmlEngine::setObjectOwnership(objectType, QQmlEngine::CppOwnership);
    return objectType;
}

AbstractNode* AnalyticsFilterModel::findFilterObjectType(const QStringList& analyticsObjectTypeIds)
{
    return objectTypeById(Node::makeId(analyticsObjectTypeIds));
}

QStringList AnalyticsFilterModel::getAnalyticsObjectTypeIds(AbstractNode* filterObjectType)
{
    if (!filterObjectType)
        return {};

    const auto ids = filterObjectType->fullSubtreeTypeIds();
    return QStringList{ids.begin(), ids.end()};
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
