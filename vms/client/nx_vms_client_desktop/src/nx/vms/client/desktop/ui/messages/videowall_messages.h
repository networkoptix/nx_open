// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <core/resource/resource_fwd.h>

class QWidget;

namespace nx::vms::client::desktop {
namespace ui {
namespace messages {

class Videowall
{
    Q_DECLARE_TR_FUNCTIONS(Videowall)
public:
    static void anotherVideoWall(QWidget* parent);

    static bool switchToVideoWallMode(QWidget* parent, bool* closeCurrentInstanse);

    /**
     * Check if resources list contains local files, which cannot be placed on the given screen.
     * Displays an error message if there are local files, and screen belongs to other pc.
     * @return true if resource placing is possible, false otherwise.
     */
    static bool checkLocalFiles(QWidget* parent,
        const QnVideoWallItemIndex& index,
        const QnResourceList& resources,
        bool displayDelayed = false);
};

} // namespace messages
} // namespace ui
} // namespace nx::vms::client::desktop
