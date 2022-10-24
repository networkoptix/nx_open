// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <set>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <nx/analytics/taxonomy/abstract_engine.h>
#include <nx/vms/client/desktop/analytics/taxonomy/abstract_node.h>
#include <nx/vms/client/desktop/analytics/taxonomy/state_view_builder.h>

namespace nx::vms::client::desktop::analytics {
class TaxonomyManager;
} // namespace nx::vms::client::desktop::analytics.

namespace nx::vms::client::desktop::analytics::taxonomy {

/**
 * Filter model represents the Client's structure of available analytics filters (engines, objects,
 * attributes). Supports additional options such as engine selection.
 */
class AnalyticsFilterModel: public QObject
{
    Q_OBJECT

public:
    Q_PROPERTY(
        std::vector<nx::vms::client::desktop::analytics::taxonomy::AbstractNode*> objectTypes
        READ objectTypes NOTIFY objectTypesChanged);

    Q_PROPERTY(std::vector<nx::analytics::taxonomy::AbstractEngine*> engines
        READ engines NOTIFY enginesChanged);

    Q_PROPERTY(QnVirtualCameraResourceSet selectedDevices WRITE setSelectedDevices)

    Q_PROPERTY(bool active WRITE setActive READ isActive NOTIFY activeChanged);

public:
    AnalyticsFilterModel(TaxonomyManager* taxonomyManager, QObject* parent = nullptr);

    /**
     * Returns filter object types structure corresponding to selected options.
     */
    std::vector<AbstractNode*> objectTypes() const;

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
     * Sets whether to exclude live types.
     */
    Q_INVOKABLE void setLiveTypesExcluded(bool value);

    /**
     * Finds the filter object type corresponding to analytics object type ids.
     */
    Q_INVOKABLE nx::vms::client::desktop::analytics::taxonomy::AbstractNode* findFilterObjectType(
        const QStringList& analyticsObjectTypeIds);

    /**
     * Returns analytics object type ids corresponding to the filter object type.
     */
    Q_INVOKABLE QStringList getAnalyticsObjectTypeIds(AbstractNode* filterObjectType);

    /**
     * Returns whether the model is active: the model is updated when taxonomy is changed.
     */
    bool isActive() const;

    /**
     * Sets whether the model is active: the model is updated when taxonomy is changed.
     */
    void setActive(bool value);

signals:
    void objectTypesChanged();
    void enginesChanged();
    void activeChanged();

private:
    AbstractNode* objectTypeById(const QString& id) const;
    void setObjectTypes(const std::vector<AbstractNode*>& objectTypes);
    void setEngines(const std::vector<nx::analytics::taxonomy::AbstractEngine*>& engines);
    void rebuild();
    void update(
        nx::analytics::taxonomy::AbstractEngine* engine,
        const std::set<QnUuid>& devices,
        bool liveTypesExcluded,
        bool force = false);

private:
    QPointer<TaxonomyManager> m_taxonomyManager;
    std::unique_ptr<StateViewBuilder> m_stateViewBuilder;
    std::vector<nx::analytics::taxonomy::AbstractEngine*> m_engines;
    QPointer<nx::analytics::taxonomy::AbstractEngine> m_engine;
    std::set<QnUuid> m_devices;
    bool m_liveTypesExcluded = false;
    std::vector<AbstractNode*> m_objectTypes;
    QMap<QString, AbstractNode*> m_objectTypesById;
    bool m_isActive = true;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy
