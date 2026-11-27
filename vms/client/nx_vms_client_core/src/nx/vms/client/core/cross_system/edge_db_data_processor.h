// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QObject>
#include <QtCore/QString>

namespace nx::vms::client::core {

class SystemContext;
class EdgeDbDataLoader;

/**
 * Creates resources from the edgeDb Organization data in the contexts of the corresponding Sites.
 * The data is read from the loader, which owns it and must outlive the processor. The resources
 * are incomplete and marked with `Qn::fake`:
 * - Devices become temporary `CrossSystemCameraResource`s, which are filled with full data once
 *   the Site is connected. Devices of already connected Sites are not created, as such Sites
 *   load their Devices themselves;
 * - Layouts become `LayoutResource`s without items, which are to be loaded when a Layout is
 *   opened. Sites do not load their Layouts even when connected, so Layouts of connected Sites
 *   are created too.
 *
 * edgeDb returns only the Sites and records available to the current User, so no access checks
 * are done here. Resources are created again each time a Site context appears, so a Site without
 * a context or with a recreated one gets them too. Resources already present in the Site resource
 * pool are not changed, except the Devices without the Site data, e.g. the ones created for
 * cross-site Layouts: they are updated with the edgeDb data.
 */
class NX_VMS_CLIENT_CORE_API EdgeDbDataProcessor: public QObject
{
    Q_OBJECT

public:
    struct SiteContext
    {
        /** Null if there is no context for the Site. */
        SystemContext* systemContext = nullptr;

        /** Whether the Site context is connected and loads the Devices from the Site itself. */
        bool hasOwnDevices = false;
    };

    /**
     * Finds the context of the Site. Set from outside only by the tests, which have no Cloud and
     * so no cross-system contexts.
     */
    using ContextResolver = std::function<SiteContext(const QString& siteId)>;

    /** Uses cross-system contexts of the Cloud Sites. */
    explicit EdgeDbDataProcessor(const EdgeDbDataLoader* loader, QObject* parent = nullptr);
    EdgeDbDataProcessor(const EdgeDbDataLoader* loader,
        ContextResolver contextResolver,
        QObject* parent = nullptr);

    /**
     * Creates the missing resources of a Site that has a new context. Does nothing until the
     * loader is ready. Connected to `CloudCrossSystemManager::systemFound()` by the first
     * constructor, public for the tests.
     */
    void onSiteContextAdded(const QString& siteId);

private:
    /** Creates the resources of all Sites. Connected to `EdgeDbDataLoader::ready()`. */
    void onDataReady();

private:
    const EdgeDbDataLoader* const m_loader;
    ContextResolver m_contextResolver;
};

} // namespace nx::vms::client::core
