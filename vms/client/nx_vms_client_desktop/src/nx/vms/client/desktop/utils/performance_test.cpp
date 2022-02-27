// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "performance_test.h"

#include <QtCore/QRegExp>
#include <QtCore/QCoreApplication>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/QOffscreenSurface>

#include <utils/common/performance.h>
#include <nx/utils/log/log.h>

#include <client/client_globals.h>
#include <client/client_runtime_settings.h>
#include <ui/dialogs/common/message_box.h>

#include <nx/branding.h>
#include <nx/build_info.h>

namespace nx::vms::client::desktop {

void PerformanceTest::detectLightMode()
{
    bool poorCpu = false;
    bool poorGpu = false;

    if (nx::build_info::isLinux())
    {
        QString cpuName = QnPerformance::cpuName();
        QRegExp poorCpuRegExp(QLatin1String("Intel\\(R\\) (Atom\\(TM\\)|Celeron\\(R\\)) CPU .*"));
        poorCpu = poorCpuRegExp.exactMatch(cpuName);
        NX_INFO(typeid(PerformanceTest), nx::format("CPU: \"%1\" poor: %2").arg(cpuName).arg(poorCpu));

        // Create OpenGL context and check GL_RENDERER.
        const auto surface = std::make_unique<QOffscreenSurface>();
        surface->create();
        const auto context = std::make_unique<QOpenGLContext>();

        if (context->create() && context->makeCurrent(surface.get()))
        {
            const auto renderer = QString::fromLatin1(reinterpret_cast<const char*>(
                context->functions()->glGetString(GL_RENDERER)));
            QRegExp poorRendererRegExp(
                QLatin1String("Gallium .* on llvmpipe .*|Mesa DRI Intel\\(R\\) Bay Trail.*"));
            poorGpu = poorRendererRegExp.exactMatch(renderer);

            NX_INFO(typeid(PerformanceTest), nx::format("Renderer: \"%1\" poor: %2").arg(renderer).arg(poorGpu));
        }
    }

    if (poorCpu && poorGpu)
    {
        qnRuntime->setLightMode(Qn::LightModeFull);

        const auto text = QCoreApplication::translate(
            "QnPerformanceTest", "%1 can work in configuration mode only")
                .arg(nx::branding::desktopClientDisplayName());

        QString extras = QCoreApplication::translate(
            "QnPerformanceTest",
            "Performance of this computer allows running %1 in configuration mode only.")
                .arg(nx::branding::desktopClientDisplayName());
        extras += L'\n';
        extras += QCoreApplication::translate(
            "QnPerformanceTest", "For full-featured mode please use another computer");

        QnMessageBox::warning(nullptr, text, extras);
    }
}

} // namespace nx::vms::client::desktop
