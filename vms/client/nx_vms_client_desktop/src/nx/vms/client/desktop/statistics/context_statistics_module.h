// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

class QnCertificateStatisticsModule;
class QnControlsStatisticsModule;
class QnStatisticsManager;

namespace nx::vms::client::desktop {

/**
 * Central place for the initialization, storage and access to various statistics modules. Part of
 * the Application Context.
*/
class ContextStatisticsModule
{
public:
    ContextStatisticsModule();
    ~ContextStatisticsModule();

    /**
     * Accessor the the single statistics module instance using Application Context.
     */
    static ContextStatisticsModule* instance();

    /**
     * Statistics manager. Stores separate statistics modules, aggregates and filters information.
     */
    QnStatisticsManager* manager() const;

    /**
     * Interface to collect and store SSL Certificate dialogs usage statistics.
     */
    QnCertificateStatisticsModule* certificates() const;

    /**
     * Interface to collect and store UI controls usage statistics.
     */
    QnControlsStatisticsModule* controls() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

inline ContextStatisticsModule* statisticsModule() { return ContextStatisticsModule::instance(); }

} // namespace nx::vms::client::desktop
