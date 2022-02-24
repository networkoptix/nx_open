// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

class QWidget;

namespace nx::vms::client::desktop {

class SharedMemoryManager;

/**
 * Manages screens, used by the application. Notifies another client instance about used screens.
 * Generates next free screen number for the newly opened client windows.
 */
class ScreenManager
{
public:
    explicit ScreenManager(SharedMemoryManager* memory);
    virtual ~ScreenManager();

    void updateCurrentScreens(const QWidget* widget);
    int nextFreeScreen() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
