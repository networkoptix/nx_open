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
#include <nx/vms/client/core/analytics/taxonomy/state_view_builder.h>

namespace nx::vms::client::core::analytics {
class TaxonomyManager;
} // namespace nx::vms::client::core::analytics.

namespace nx::vms::client::core::analytics::taxonomy {

class AbstractStateViewFilter;

/**
 * Filter model represents the Client's structure of available analytics filters (engines, objects,
 * attributes). Supports additional options such as engine selection.
 */
class NX_VMS_CLIENT_CORE_API AnalyticsFilterModel: public QObject
{
    Q_OBJECT

public:
    Q_PROPERTY(std::vector<nx::vms::client::core::analytics::taxonomy::ObjectType*> objectTypes
        READ objectTypes
        NOTIFY objectTypesChanged);

    Q_PROPERTY(nx::analytics::taxonomy::AbstractEngine* selectedEngine
        READ selectedEngine
        WRITE setSelectedEngine
        NOTIFY selectedEngineChanged)

    Q_PROPERTY(QnVirtualCameraResourceSet selectedDevices
        READ selectedDevices
        WRITE setSelectedDevices
        NOTIFY selectedDevicesChanged)

    Q_PROPERTY(QVariantMap selectedAttributeValues
        READ selectedAttributeValues
        WRITE setSelectedAttributeValues
        NOTIFY selectedAttributeValuesChanged)

    Q_PROPERTY(std::vector<nx::analytics::taxonomy::AbstractEngine*> engines
        READ engines
        NOTIFY enginesChanged);

    Q_PROPERTY(bool active WRITE setActive READ isActive NOTIFY activeChanged);

public:
    static void registerQmlType();

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
    void setSelectedEngine(nx::analytics::taxonomy::AbstractEngine* engine);
    nx::analytics::taxonomy::AbstractEngine* selectedEngine() const;

    /**
     * Sets selected devices.
     */
    QnVirtualCameraResourceSet selectedDevices() const;
    void setSelectedDevices(const QnVirtualCameraResourceSet& devices);
    void setSelectedDevices(const std::set<QnUuid>& deviceIds);

    /**
     * Sets selected attribute values.
     */
    void setSelectedAttributeValues(const QVariantMap& values);
    QVariantMap selectedAttributeValues() const;

    /**
     * Sets whether to exclude live types.
     */
    Q_INVOKABLE void setLiveTypesExcluded(bool value);

    /**
     * Finds the filter object type corresponding to analytics object type ids.
     */
    Q_INVOKABLE nx::vms::client::core::analytics::taxonomy::ObjectType* findFilterObjectType(
        const QStringList& analyticsObjectTypeIds);

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
    void selectedEngineChanged();
    void selectedDevicesChanged();
    void selectedAttributeValuesChanged();

private:
    ObjectType* objectTypeById(const QString& id) const;
    void setObjectTypes(const std::vector<ObjectType*>& objectTypes);
    void setEngines(const std::vector<nx::analytics::taxonomy::AbstractEngine*>& engines);
    void rebuild();
    void update(
        nx::analytics::taxonomy::AbstractEngine* engine,
        const QnVirtualCameraResourceSet& devices,
        const QVariantMap& attributeValues,
        bool liveTypesExcluded,
        bool force = false);

private:
    QPointer<TaxonomyManager> m_taxonomyManager;
    nx::utils::ScopedConnection m_manifestsUpdatedConnection;
    std::unique_ptr<StateViewBuilder> m_stateViewBuilder;
    std::vector<nx::analytics::taxonomy::AbstractEngine*> m_engines;

    QPointer<nx::analytics::taxonomy::AbstractEngine> m_engine;
    QnVirtualCameraResourceSet m_devices;
    QVariantMap m_attributeValues;
    bool m_liveTypesExcluded = false;

    std::vector<ObjectType*> m_objectTypes;
    QMap<QString, ObjectType*> m_objectTypesById;
    bool m_isActive = true;
};

} // namespace nx::vms::client::core::analytics::taxonomy
