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

void mac_setLimits();

QString mac_getExistingDirectory(const QString &caption,
                                    const QString &dir);

QString mac_getOpenFileName(const QString &caption,
                                 const QString &dir,
                                 const QStringList &extensions);

QStringList mac_getOpenFileNames(const QString &caption,
                                 const QString &dir,
                                 const QStringList &extensions);

QString mac_getSaveFileName(const QString &caption,
                            const QString &dir,
                            const QStringList &extensions);

#endif // _HDWITNESS_MAC_UTILS_H_
