// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <set>
#include <vector>

#include <QtCore/QMetaObject>
#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/analytics/taxonomy/abstract_engine.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/desktop/analytics/taxonomy/state_view_builder.h>

Q_MOC_INCLUDE("nx/vms/client/desktop/analytics/taxonomy/object_type.h")

namespace nx::vms::client::desktop::analytics {
class TaxonomyManager;
} // namespace nx::vms::client::desktop::analytics.

namespace nx::vms::client::desktop::analytics::taxonomy {

class AbstractStateViewFilter;

/**
 * Filter model represents the Client's structure of available analytics filters (engines, objects,
 * attributes). Supports additional options such as engine selection.
 */
class AnalyticsFilterModel: public QObject
{
    Q_OBJECT

public:
    Q_PROPERTY(std::vector<nx::vms::client::desktop::analytics::taxonomy::ObjectType*> objectTypes
        READ objectTypes
        NOTIFY objectTypesChanged);

    Q_PROPERTY(std::vector<nx::analytics::taxonomy::AbstractEngine*> engines
        READ engines
        NOTIFY enginesChanged);

    Q_PROPERTY(bool active WRITE setActive READ isActive NOTIFY activeChanged);

public:
    AnalyticsFilterModel(TaxonomyManager* taxonomyManager = nullptr, QObject* parent = nullptr);

    /**
     * Returns filter object types structure corresponding to selected options.
     */
    std::vector<ObjectType*> objectTypes() const;

    /**
     * Returns engines.
     */
    std::vector<nx::analytics::taxonomy::AbstractEngine*> engines() const;

    /**
     * Sets selected engine.
     */
    Q_INVOKABLE void setSelectedEngine(
        nx::analytics::taxonomy::AbstractEngine* engine,
        bool force = false);

    /**
     * Sets selected devices.
     */
    Q_INVOKABLE void setSelectedDevices(const QnVirtualCameraResourceSet& devices);

    /**
     * Sets selected devices.
     */
    void setSelectedDevices(const std::set<nx::Uuid>& devices);

    /**
     * Sets selected attribute values.
     */
    Q_INVOKABLE void setSelectedAttributeValues(const QVariantMap& values);

    /**
     * Sets whether to exclude live types.
     */
    Q_INVOKABLE void setLiveTypesExcluded(bool value);

    /**
     * Finds the filter object type corresponding to analytics object type ids.
     */
    Q_INVOKABLE nx::vms::client::desktop::analytics::taxonomy::ObjectType* findFilterObjectType(
        const QStringList& analyticsObjectTypeIds);
    /**
     * Finds engine by id.
     */
    Q_INVOKABLE nx::analytics::taxonomy::AbstractEngine* findEngine(const nx::Uuid& engineId) const;

    /**
     * Returns analytics object type ids corresponding to the filter object type.
     */
    Q_INVOKABLE QStringList getAnalyticsObjectTypeIds(ObjectType* filterObjectType);

    /**
     * Returns whether the model is active: the model is updated when the taxonomy is changed.
     */
    bool isActive() const;

    /**
     * Sets whether the model is active: the model is updated when the taxonomy is changed.
     */
    void setActive(bool value);

    /**
     * Sets selected engine and attribute values.
     */
    Q_INVOKABLE void setSelected(
        nx::analytics::taxonomy::AbstractEngine* engine,
        const QVariantMap& attributeValues);

signals:
    void objectTypesChanged();
    void enginesChanged();
    void activeChanged();

private:
    ObjectType* objectTypeById(const QString& id) const;
    void setObjectTypes(const std::vector<ObjectType*>& objectTypes);
    void setEngines(const std::vector<nx::analytics::taxonomy::AbstractEngine*>& engines);
    void rebuild();
    void update(
        nx::analytics::taxonomy::AbstractEngine* engine,
        const std::set<nx::Uuid>& devices,
        const QVariantMap& attributeValues,
        bool liveTypesExcluded,
        bool force = false);

private:
    QPointer<TaxonomyManager> m_taxonomyManager;
    nx::utils::ScopedConnection m_manifestsUpdatedConnection;
    std::unique_ptr<StateViewBuilder> m_stateViewBuilder;
    std::vector<nx::analytics::taxonomy::AbstractEngine*> m_engines;
    QMap<nx::Uuid, nx::analytics::taxonomy::AbstractEngine*> m_enginesById;

    QPointer<nx::analytics::taxonomy::AbstractEngine> m_engine;
    std::set<nx::Uuid> m_devices;
    QVariantMap m_attributeValues;
    bool m_liveTypesExcluded = false;

    std::vector<ObjectType*> m_objectTypes;
    QMap<QString, ObjectType*> m_objectTypesById;
    bool m_isActive = true;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy
