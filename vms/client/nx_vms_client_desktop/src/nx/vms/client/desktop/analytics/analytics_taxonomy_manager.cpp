// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_taxonomy_manager.h"

#include <QtQml/QtQml>

#include <client/client_module.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/utils/range_adapters.h>

namespace nx::vms::client::desktop {
namespace analytics {

using namespace nx::analytics::taxonomy;

struct TaxonomyManager::Private
{
    mutable std::optional<std::shared_ptr<Taxonomy>> currentTaxonomy;
    mutable std::optional<QHash<AbstractEngine*, QSet<AbstractObjectType*>>> relevantObjectTypes;
    mutable QSet<AbstractEngine*> relevantEngines;

    void reset()
    {
        relevantEngines.clear();
        relevantObjectTypes = std::nullopt;

        // Destruct taxonomy object only after currentTaxonomy variable is cleared.
        decltype(currentTaxonomy) taxonomyReleaser;
        std::swap(currentTaxonomy, taxonomyReleaser);
    }

    void ensureTaxonomy(AbstractStateWatcher* stateWatcher) const
    {
        if (currentTaxonomy.has_value())
            return;

        currentTaxonomy = NX_ASSERT(stateWatcher) ? stateWatcher->state() : nullptr;
        if (currentTaxonomy.value())
            QQmlEngine::setObjectOwnership(currentTaxonomy.value().get(), QQmlEngine::CppOwnership);
    }

    void ensureRelevancyInfo(AbstractStateWatcher* stateWatcher) const
    {
        if (relevantObjectTypes.has_value())
            return;

        relevantObjectTypes = QHash<AbstractEngine*, QSet<AbstractObjectType*>>();

        ensureTaxonomy(stateWatcher);
        if (!*currentTaxonomy)
            return;

        // If an engine detects an objectType, it's added to the objectType's scoped array.
        // However it's not added to all base types' scopes.

        // Here we build a fast lookup dictionaries to check whether an objectType or any of its
        // descendants are detected by a given engine and to check if a given engine detects any
        // public object types.

        QSet<AbstractObjectType*> publicRootTypes;
        for (const auto objectType: (*currentTaxonomy)->rootObjectTypes())
        {
            if (objectType->isReachable())
                publicRootTypes.insert(objectType);
        }

        for (const auto objectType: (*currentTaxonomy)->objectTypes())
        {
            const bool wasUsed = objectType->hasEverBeenSupported()
                && !objectType->isLiveOnly()
                && !objectType->isNonIndexable();

            for (const auto scope: objectType->scopes())
            {
                auto& relevantTypes = (*relevantObjectTypes)[scope->engine()];

                for (auto currentType = objectType;
                    currentType && !relevantTypes.contains(currentType);
                    currentType = currentType->base())
                {
                    relevantTypes.insert(currentType);

                    if (wasUsed && publicRootTypes.contains(currentType))
                        relevantEngines.insert(scope->engine());
                }
            }
        }

        QSet<AbstractObjectType*> allRelevantTypes;
        for (const auto& [engine, types]: nx::utils::constKeyValueRange(*relevantObjectTypes))
        {
            if (relevantEngines.contains(engine))
                allRelevantTypes += types;
        }

        (*relevantObjectTypes)[nullptr] = allRelevantTypes;
    }
};

TaxonomyManager::TaxonomyManager(QnCommonModule* commonModule, QObject* parent):
    QObject(parent),
    QnCommonModuleAware(commonModule),
    d(new Private())
{
    const auto taxonomyStateWatcher = commonModule ? commonModule->taxonomyStateWatcher() : nullptr;
    if (!NX_CRITICAL(taxonomyStateWatcher, "Common module singletons must be initialized"))
        return;

    connect(taxonomyStateWatcher, &AbstractStateWatcher::stateChanged, this,
        [this]()
        {
            d->reset();
            emit currentTaxonomyChanged();
        });
}

TaxonomyManager::~TaxonomyManager()
{
    // Required here for forward-declared scoped pointer destruction.
}

Taxonomy* TaxonomyManager::currentTaxonomy() const
{
    d->ensureTaxonomy(commonModule()->taxonomyStateWatcher());
    return d->currentTaxonomy.value().get();
}

QVariant TaxonomyManager::objectTypeById(const QString& objectTypeId) const
{
    const auto taxonomy = currentTaxonomy();
    return taxonomy
        ? QVariant::fromValue(taxonomy->objectTypeById(objectTypeId))
        : QVariant();
}

bool TaxonomyManager::isEngineRelevant(AbstractEngine* engine) const
{
    if (!NX_ASSERT(engine))
        return false;

    d->ensureRelevancyInfo(commonModule()->taxonomyStateWatcher());
    return d->relevantEngines.contains(engine);
}

bool TaxonomyManager::isRelevantForEngine(AbstractObjectType* type, AbstractEngine* engine) const
{
    if (!NX_ASSERT(type))
        return false;

    d->ensureRelevancyInfo(commonModule()->taxonomyStateWatcher());
    return d->relevantObjectTypes->value(engine).contains(type);
}

QSet<nx::analytics::taxonomy::AbstractEngine*> TaxonomyManager::relevantEngines() const
{
    d->ensureRelevancyInfo(commonModule()->taxonomyStateWatcher());
    return d->relevantEngines;
}

QSet<AbstractObjectType*> TaxonomyManager::relevantObjectTypes(AbstractEngine* engine) const
{
    d->ensureRelevancyInfo(commonModule()->taxonomyStateWatcher());
    return d->relevantObjectTypes->value(engine);
}

void TaxonomyManager::registerQmlTypes()
{
    qmlRegisterSingletonType<TaxonomyManager>(
        "nx.vms.client.desktop.analytics", 1, 0, "TaxonomyManager",
        [](QQmlEngine* qmlEngine, QJSEngine* /*jsEngine*/) -> QObject*
        {
            return new TaxonomyManager(qnClientModule->clientCoreModule()->commonModule());
        });

    qmlRegisterUncreatableType<AbstractState>(
        "nx.vms.client.desktop.analytics", 1, 0, "Taxonomy",
        "Cannot create an instance of Taxonomy");

    qmlRegisterUncreatableType<AbstractEngine>(
        "nx.vms.client.desktop.analytics", 1, 0, "Engine",
        "Cannot create an instance of Engine");

    qmlRegisterUncreatableType<AbstractObjectType>(
        "nx.vms.client.desktop.analytics", 1, 0, "ObjectType",
        "Cannot create an instance of ObjectType");

    qmlRegisterUncreatableType<AbstractEnumType>(
        "nx.vms.client.desktop.analytics", 1, 0, "EnumType",
        "Cannot create an instance of EnumType");

    qmlRegisterUncreatableType<AbstractAttribute>(
        "nx.vms.client.desktop.analytics", 1, 0, "Attribute",
        "Cannot create an instance of Attribute");

    qmlRegisterUncreatableType<AbstractScope>(
        "nx.vms.client.desktop.analytics", 1, 0, "Scope",
        "Cannot create an instance of Scope");

    qmlRegisterUncreatableType<AbstractGroup>(
        "nx.vms.client.desktop.analytics", 1, 0, "Group",
        "Cannot create an instance of Group");
}

} // namespace analytics
} // namespace nx::vms::client::desktop
