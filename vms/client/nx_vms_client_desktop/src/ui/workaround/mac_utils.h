#ifndef _HDWITNESS_MAC_UTILS_H_
#define _HDWITNESS_MAC_UTILS_H_

void mac_initFullScreen(void *winId, void *qnmainwindow);
void mac_showFullScreen(void *winId, bool);
bool mac_isFullscreen(void *winId);
void mac_disableFullscreenButton(void *winId);

void mac_saveFileBookmark(const QString& path);

void mac_restoreFileAccess();
void mac_stopFileAccess();

bool mac_isSandboxed();

void setHidesOnDeactivate(WId windowId, bool value);

#endif // _HDWITNESS_MAC_UTILS_H_
