#pragma once

#include <text/tr_functions.h>

#include <core/resource/resource_fwd.h>

class QWidget;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace videowall {

QN_DECLARE_TR_FUNCTIONS("nx::client::desktop::ui::videowall")

void anotherVideoWall(QWidget* parent);

bool switchToVideoWallMode(QWidget* parent, bool* closeCurrentInstanse);

/**
 * Check if resources list contains local files, which cannot be placed on the given screen.
 * Displays an error message if there are local files, and screen belongs to other pc.
 * @return true if resource placing is possible, false otherwise.
 */
bool checkLocalFiles(QWidget* parent,
    const QnVideoWallItemIndex& index,
    const QnResourceList& resources,
    bool displayDelayed = false);

} // namespace videowall
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
