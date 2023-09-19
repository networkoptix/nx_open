// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

class QWidget;
class QString;

using TransitionStateCallback = std::function<void (bool /*inProgress*/)>;
void setFullscreenTransitionHandler(QWidget* widget, TransitionStateCallback callback);

void mac_showFullScreen(void *winId, bool);
bool mac_isFullscreen(void *winId);
void mac_disableFullscreenButton(void *winId);

void mac_saveFileBookmark(const QString& path);

void mac_restoreFileAccess();
void mac_stopFileAccess();

bool mac_isSandboxed();

void setHidesOnDeactivate(WId windowId, bool value);
