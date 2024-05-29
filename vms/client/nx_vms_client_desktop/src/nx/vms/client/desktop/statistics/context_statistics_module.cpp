// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "context_statistics_module.h"

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <statistics/statistics_manager.h>
#include <statistics/storage/statistics_file_storage.h>
#include <ui/statistics/modules/certificate_statistics_module.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/statistics/modules/generic_statistics_module.h>
#include <ui/statistics/modules/session_restore_statistics_module.h>

#include "context_statistics_module.h"
#include "modules/build_info_statistics_module.h"
#include "modules/os_statistics_module.h"

namespace nx::vms::client::desktop {

struct ContextStatisticsModule::Private
{
    std::unique_ptr<QnStatisticsManager> manager;
    std::unique_ptr<BuildInfoStatisticsModule> buildInfoModule =
        std::make_unique<BuildInfoStatisticsModule>();
    std::unique_ptr<OsStatisticsModule> osModule =
        std::make_unique<OsStatisticsModule>();
    std::unique_ptr<QnCertificateStatisticsModule> certificatesModule;
    std::unique_ptr<QnControlsStatisticsModule> controlsModule;
    std::unique_ptr<QnGenericStatisticsModule> genericModule;
    std::unique_ptr<QnSessionRestoreStatisticsModule> sessionRestoreModule;
};

ContextStatisticsModule::ContextStatisticsModule():
    d(new Private())
{
    d->manager = std::make_unique<QnStatisticsManager>();
    d->manager->setClientId(appContext()->localSettings()->pcUuid());
    d->manager->setStorage(std::make_unique<QnStatisticsFileStorage>());

    d->manager->registerStatisticsModule("os", d->osModule.get());
    d->manager->registerStatisticsModule("build", d->buildInfoModule.get());

    d->genericModule = std::make_unique<QnGenericStatisticsModule>();
    d->manager->registerStatisticsModule("generic", d->genericModule.get());

    d->certificatesModule = std::make_unique<QnCertificateStatisticsModule>();
    d->manager->registerStatisticsModule("certificate", d->certificatesModule.get());

    d->controlsModule = std::make_unique<QnControlsStatisticsModule>();
    d->manager->registerStatisticsModule("controls", d->controlsModule.get());

    d->sessionRestoreModule = std::make_unique<QnSessionRestoreStatisticsModule>();
    d->manager->registerStatisticsModule("restore", d->sessionRestoreModule.get());

    appContext()->clientStateHandler()->setStatisticsModules(
        d->manager.get(),
        d->sessionRestoreModule.get());
}

ContextStatisticsModule::~ContextStatisticsModule()
{
}

ContextStatisticsModule* ContextStatisticsModule::instance()
{
    return appContext()->statisticsModule();
}

QnStatisticsManager* ContextStatisticsModule::manager() const
{
    return d->manager.get();
}

QnCertificateStatisticsModule* ContextStatisticsModule::certificates() const
{
    return d->certificatesModule.get();
}

QnControlsStatisticsModule* ContextStatisticsModule::controls() const
{
    return d->controlsModule.get();
}

} // namespace nx::vms::client::desktop
