// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QSet>

#include <common/common_module_aware.h>
#include <nx/analytics/taxonomy/abstract_engine.h>
#include <nx/analytics/taxonomy/abstract_object_type.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/system_context_aware.h>

#include "taxonomy/state_view.h"

class QnCommonModule;

namespace nx::vms::client::core {
namespace analytics {
namespace taxonomy {
class AnalyticsFilterModel;
} // namespace taxonomy

using Taxonomy = nx::analytics::taxonomy::AbstractState;

class NX_VMS_CLIENT_CORE_API TaxonomyManager:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    Q_PROPERTY(nx::analytics::taxonomy::AbstractState* currentTaxonomy
        READ qmlCurrentTaxonomy NOTIFY currentTaxonomyChanged)

public:
    explicit TaxonomyManager(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~TaxonomyManager() override;

    static void registerQmlTypes();

    std::shared_ptr<Taxonomy> currentTaxonomy() const;
    Taxonomy* qmlCurrentTaxonomy() const;
    Q_INVOKABLE nx::vms::client::core::analytics::taxonomy::AnalyticsFilterModel*
        createFilterModel(QObject* parent = nullptr);

    Q_INVOKABLE nx::vms::client::core::analytics::taxonomy::StateView*
        createStateView(QObject* parent = nullptr) const;

    Q_INVOKABLE QVariant objectTypeById(const QString& objectTypeId) const;

    Q_INVOKABLE bool isEngineRelevant(nx::analytics::taxonomy::AbstractEngine* engine) const;

    Q_INVOKABLE bool isRelevantForEngine(nx::analytics::taxonomy::AbstractObjectType* type,
        nx::analytics::taxonomy::AbstractEngine* engine) const;

    QSet<nx::analytics::taxonomy::AbstractEngine*> relevantEngines() const;

    QSet<nx::analytics::taxonomy::AbstractObjectType*> relevantObjectTypes(
        nx::analytics::taxonomy::AbstractEngine* engine) const;

signals:
    void currentTaxonomyAboutToBeChanged(QPrivateSignal);
    void currentTaxonomyChanged(QPrivateSignal);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace analytics
} // namespace nx::vms::client::core
