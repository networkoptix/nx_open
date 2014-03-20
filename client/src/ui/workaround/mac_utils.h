#ifndef _HDWITNESS_MAC_UTILS_H_
#define _HDWITNESS_MAC_UTILS_H_

void mac_initFullScreen(void *winId, void *qnmainwindow);
void mac_showFullScreen(void *winId, bool);
bool mac_isFullscreen(void *winId);

void mac_saveFileBookmark(const QString& path);

void mac_restoreFileAccess();
void mac_stopFileAccess();

QString mac_getExistingDirectory(QWidget *parent,
                                    const QString &caption,
                                    const QString &dir,
                                    QFileDialog::Options options);

QString mac_getOpenFileName(QWidget *parent,
                                 const QString &caption,
                                 const QString &dir,
                                 const QStringList &extensions,
                                 QFileDialog::Options options);

QStringList mac_getOpenFileNames(QWidget *parent,
                                 const QString &caption,
                                 const QString &dir,
                                 const QStringList &extensions,
                                 QFileDialog::Options options);

QString mac_getSaveFileName(QWidget *parent,
                            const QString &caption,
                            const QString &dir,
                            const QStringList &extensions,
                            QFileDialog::Options options);

#endif // _HDWITNESS_MAC_UTILS_H_
