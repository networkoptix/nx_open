// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_helpers.h"

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

#include <nx/vms/client/mobile/window_context.h>
#include <nx/vms/client/mobile/utils/navigation_bar_utils.h>
#include <ui/window_utils.h>

#include "measurements.h"
#include "../ui_controller.h"

namespace nx::vms::client::mobile::detail {

WindowHelpers::WindowHelpers(WindowContext* context,
    QObject* parent)
    :
    base_class(parent),
    WindowContextAware(context)
{
}

WindowHelpers::~WindowHelpers()
{
}

int WindowHelpers::navigationBarType() const
{
    return NavigationBarUtils::navigationBarType();
}

int WindowHelpers::navigationBarSize() const
{
    return NavigationBarUtils::barSize();
}

void WindowHelpers::enterFullscreen() const
{
    hideSystemUi();
}

void WindowHelpers::exitFullscreen() const
{
    showSystemUi();
}

void WindowHelpers::copyToClipboard(const QString &text)
{
    qApp->clipboard()->setText(text);
}

bool WindowHelpers::isRunOnPhone() const
{
    return isPhone();
}

void WindowHelpers::setKeepScreenOn(bool keepScreenOn) const
{
    ::setKeepScreenOn(keepScreenOn);
}

void WindowHelpers::setScreenOrientation(Qt::ScreenOrientation orientation) const
{
    ::setScreenOrientation(orientation);
}

void WindowHelpers::setGestureExclusionArea(int y, int height) const
{
    ::setAndroidGestureExclusionArea(y, height);
}

void WindowHelpers::makeShortVibration() const
{
    ::makeShortVibration();
}

void WindowHelpers::requestRecordAudioPermissionIfNeeded() const
{
    ::requestRecordAudioPermissionIfNeeded();
}

} // namespace nx::vms::client::mobile::detail
