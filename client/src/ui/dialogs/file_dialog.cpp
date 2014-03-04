#include "ui/dialogs/file_dialog.h"

#ifdef Q_OS_MAC
#include "ui/workaround/mac_utils.h"
#endif

QString QnFileDialog::getExistingDirectory(QWidget *parent,
                                        const QString &caption,
                                        const QString &dir,
                                        Options options) {
#ifdef Q_OS_MAC
    return mac_getExistingDirectory(parent, caption, dir, options);
#else
    return QFileDialog::getExistingDirectory(parent, caption, dir, options);
#endif
}

QString QnFileDialog::getOpenFileName(QWidget *parent,
                                 const QString &caption,
                                 const QString &dir,
                                 const QString &filter,
                                 const QStringList &extensions,
                                 QString * selectedFilter,
                                 QFileDialog::Options options) {
#ifdef Q_OS_MAC
    return mac_getOpenFileName(parent, caption, dir, extensions, options);
#else
    return QFileDialog::getOpenFileName(parent, caption, dir, filter, selectedFilter, options);
#endif
}
