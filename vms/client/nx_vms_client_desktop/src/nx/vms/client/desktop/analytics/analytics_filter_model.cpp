// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_filter_model.h"

#include <QtQml/QQmlEngine>

#include <core/resource/camera_resource.h>
#include <nx/vms/client/desktop/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/desktop/analytics/taxonomy/abstract_state_view_filter.h>
#include <nx/vms/client/desktop/analytics/taxonomy/node.h>
#include <nx/vms/client/desktop/analytics/taxonomy/scope_state_view_filter.h>

namespace nx::vms::client::desktop::analytics::taxonomy {

namespace {

class Filter: public ScopeStateViewFilter
{
public:
    Filter(
        nx::analytics::taxonomy::AbstractEngine* engine,
        const std::set<QnUuid>& devices,
        bool excludeLiveTypes,
        QObject* parent = nullptr);

    virtual QString id() const override;

    virtual bool matches(
        const nx::analytics::taxonomy::AbstractObjectType* objectType) const override;

private:
    bool m_liveTypesExcluded = false;
};

Filter::Filter(
    nx::analytics::taxonomy::AbstractEngine* engine,
    const std::set<QnUuid>& devices,
    bool excludeLiveTypes,
    QObject* parent)
    :
    ScopeStateViewFilter(engine, devices, parent),
    m_liveTypesExcluded(excludeLiveTypes)
{
}

QString Filter::id() const
{
    return ScopeStateViewFilter::id() + QString::number(m_liveTypesExcluded);
}

bool Filter::matches(const nx::analytics::taxonomy::AbstractObjectType* objectType) const
{
    return ScopeStateViewFilter::matches(objectType)
        && !(m_liveTypesExcluded && (objectType->isLiveOnly() || objectType->isNonIndexable()));
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
        if (auto engineFilter = dynamic_cast<ScopeStateViewFilter*>(filter))
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

} // namespace

AnalyticsFilterModel::AnalyticsFilterModel(TaxonomyManager* taxonomyManager, QObject* parent):
    QObject(parent),
    m_taxonomyManager(taxonomyManager)
{
    if (!NX_ASSERT(taxonomyManager))
        return;

    connect(taxonomyManager, &TaxonomyManager::currentTaxonomyChanged,
        this, &AnalyticsFilterModel::rebuild);

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
    bool liveTypesExcluded,
    bool force)
{
    if (force
        || m_engine != engine
        || m_devices != devices
        || m_liveTypesExcluded != liveTypesExcluded)
    {
        m_engine = engine;
        m_devices = devices;
        m_liveTypesExcluded = liveTypesExcluded;

        const auto filter = std::make_unique<Filter>(m_engine, m_devices, m_liveTypesExcluded);
        taxonomy::AbstractStateView* state = m_stateViewBuilder->stateView(filter.get());
        setObjectTypes(state->rootNodes());
    }
}

void AnalyticsFilterModel::setSelectedEngine(
    nx::analytics::taxonomy::AbstractEngine* engine,
    bool force)
{
    update(engine, m_devices, m_liveTypesExcluded);
}

void AnalyticsFilterModel::setSelectedDevices(const QnVirtualCameraResourceSet& devices)
{
    setSelectedDevices(deviceIds(devices));
}

void AnalyticsFilterModel::setSelectedDevices(const std::set<QnUuid>& devices)
{
    update(m_engine, devices, m_liveTypesExcluded);
}

void AnalyticsFilterModel::setLiveTypesExcluded(bool value)
{
    update(m_engine, m_devices, value);
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

void AnalyticsFilterModel::rebuild()
{
    m_stateViewBuilder =
        std::make_unique<taxonomy::StateViewBuilder>(m_taxonomyManager->currentTaxonomy());

    setEngines(enginesFromFilters(m_stateViewBuilder->engineFilters()));
    update(m_engine, m_devices, m_liveTypesExcluded, /*force*/ true);
}

} // namespace nx::vms::client::desktop::analytics::taxonomy
