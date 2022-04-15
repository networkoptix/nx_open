// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_context.h"

#include <core/resource/resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/common/system_context.h>
#include <ui/statistics/modules/certificate_statistics_module.h>
#include <ui/statistics/modules/controls_statistics_module.h>

#include "system_context.h"
#include "window_context.h"

namespace nx::vms::client::desktop {

static ApplicationContext* s_instance = nullptr;

struct ApplicationContext::Private
{
    std::vector<QPointer<SystemContext>> systemContexts;
    std::vector<QPointer<WindowContext>> windowContexts;

    std::unique_ptr<QnCertificateStatisticsModule> certificateStatisticsModule;
    std::unique_ptr<QnControlsStatisticsModule> controlsStatisticsModule;
};

ApplicationContext::ApplicationContext(QObject* parent):
    QObject(parent),
    d(new Private())
{
    if (NX_ASSERT(!s_instance))
        s_instance = this;

    d->certificateStatisticsModule = std::make_unique<QnCertificateStatisticsModule>();
    d->controlsStatisticsModule = std::make_unique<QnControlsStatisticsModule>();
}

ApplicationContext::~ApplicationContext()
{
    if (NX_ASSERT(s_instance == this))
        s_instance = nullptr;
}

ApplicationContext* ApplicationContext::instance()
{
    NX_ASSERT(s_instance);
    return s_instance;
}

SystemContext* ApplicationContext::currentSystemContext() const
{
    if (NX_ASSERT(!d->systemContexts.empty()))
        return d->systemContexts.front();

    return nullptr;
}

std::vector<SystemContext*> ApplicationContext::systemContexts() const
{
    std::vector<SystemContext*> result;
    for (auto context: d->systemContexts)
    {
        if (NX_ASSERT(context))
            result.push_back(context.data());
    }
    return result;
}

void ApplicationContext::addSystemContext(SystemContext* systemContext)
{
    d->systemContexts.push_back(systemContext);
}

void ApplicationContext::removeSystemContext(SystemContext* systemContext)
{
    auto iter = std::find(d->systemContexts.begin(), d->systemContexts.end(), systemContext);
    if (NX_ASSERT(iter != d->systemContexts.end()))
        d->systemContexts.erase(iter);
}

WindowContext* ApplicationContext::mainWindowContext() const
{
    if (NX_ASSERT(!d->windowContexts.empty()))
        return d->windowContexts.front();

    return nullptr;
}

void ApplicationContext::addWindowContext(WindowContext* windowContext)
{
    d->windowContexts.push_back(windowContext);
}

void ApplicationContext::removeWindowContext(WindowContext* windowContext)
{
    auto iter = std::find(d->windowContexts.begin(), d->windowContexts.end(), windowContext);
    if (NX_ASSERT(iter != d->windowContexts.end()))
        d->windowContexts.erase(iter);
}

QnCertificateStatisticsModule* ApplicationContext::certificateStatisticsModule() const
{
    return d->certificateStatisticsModule.get();
}

QnControlsStatisticsModule* ApplicationContext::controlsStatisticsModule() const
{
    return d->controlsStatisticsModule.get();
}

} // namespace nx::vms::client::desktop
