// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "screen_manager.h"

#include <chrono>

#include <QtCore/QRect>
#include <QtCore/QSet>
#include <QtCore/QPointer>
#include <QtGui/QScreen>
#include <QtWidgets/QWidget>
#include <QtWidgets/QApplication>

#include <nx/vms/client/desktop/state/shared_memory_manager.h>

#include <utils/screen_utils.h>

#include <nx/utils/pending_operation.h>
#include <nx/branding.h>

using namespace std::chrono;

namespace nx::vms::client::desktop {

namespace {

// Avoid writing to shared memory on each geometry update - while dragging or resizing window.
static const milliseconds kStoreScreensDelay = 1s;

} // namespace

struct ScreenManager::Private
{
    Private(SharedMemoryManager* memoryManager):
        memory(memoryManager)
    {
        storeScreensOperation.setInterval(kStoreScreensDelay);
        storeScreensOperation.setCallback(
            [this]()
            {
                if (memory)
                    memory->setCurrentInstanceScreens(localScreens);
            });
    }

    void storeScreens()
    {
        using nx::gui::Screens;
        localScreens = Screens::coveredBy(geometry, Screens::physicalGeometries());
        storeScreensOperation.requestOperation();
    }

    QPointer<SharedMemoryManager> memory;
    nx::utils::PendingOperation storeScreensOperation;
    QSet<int> localScreens;

    /** Widget geometry converted to physical pixels. */
    QRect geometry;
};

ScreenManager::ScreenManager(SharedMemoryManager* memory):
    d(new Private(memory))
{
}

ScreenManager::~ScreenManager()
{
}

void ScreenManager::updateCurrentScreens(const QWidget *widget)
{
    d->geometry = widget->geometry();

    // QWidget::geometry() returns QRect with logical size and physical position. We have to
    // convert size to physical pixels too.
    const QList<QScreen*>& screens = QApplication::screens();
    const auto screenIndex = screens.indexOf(widget->screen());
    if (screenIndex >= 0 && screenIndex < screens.size())
        d->geometry.setSize(d->geometry.size() * screens[screenIndex]->devicePixelRatio());

    d->storeScreens();
}

int ScreenManager::nextFreeScreen() const
{
    QSet<int> current = d->localScreens;
    int currentScreen = current.isEmpty() ? 0 : *std::min_element(current.begin(), current.end());

    int screenCount = nx::gui::Screens::count();
    int nextScreen = (currentScreen + 1) % screenCount;

    QSet<int> used = d->memory->allUsedScreens();
    for (int i = 0; i < screenCount; i++)
    {
        int screen = (nextScreen + i) % screenCount;
        if (!used.contains(screen))
            return screen;
    }

    return -1;
}

} // namespace nx::vms::client::desktop
