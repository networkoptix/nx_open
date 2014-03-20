#include "ui/dialogs/file_dialog.h"

#include <QtCore/QSet>

#ifdef Q_OS_MAC
#include "ui/workaround/mac_utils.h"
#endif

namespace {
    QStringList getExtensionsFromFilter(const QString &filter) {
        QRegExp filterRegExp(lit(".*\\((.*)\\).*"));
        QRegExp extensionRegExp(lit("\\*\\.(.*)"));

        QSet<QString> extensions;
        QStringList filters = filter.split(lit(";;"), QString::SkipEmptyParts);
        foreach (const QString &filter, filters) {
            if (filterRegExp.exactMatch(filter)) {
                foreach (const QString &extension, filterRegExp.cap(1).split(QChar::fromLatin1(' '))) {
                    if (extensionRegExp.exactMatch(extension))
                        extensions.insert(extensionRegExp.cap(1));
                }
            }
        }
        extensions.remove(lit("*"));

        return extensions.toList();
    }
}

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
                                 QString *selectedFilter,
                                 QFileDialog::Options options) {
#ifdef Q_OS_MAC
    return mac_getOpenFileName(parent, caption, dir, getExtensionsFromFilter(filter), options);
#else
    return QFileDialog::getOpenFileName(parent, caption, dir, filter, selectedFilter, options);
#endif
}

QString QnFileDialog::getSaveFileName(QWidget *parent, const QString &caption, const QString &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options)
{
#ifdef Q_OS_MAC
    return mac_getSaveFileName(parent, caption, dir, getExtensionsFromFilter(filter), options);
#else
    return QFileDialog::getSaveFileName(parent, caption, dir, filter, selectedFilter, options);
#endif
}

QStringList QnFileDialog::getOpenFileNames(QWidget *parent, const QString &caption, const QString &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options)
{
#ifdef Q_OS_MAC
    return mac_getOpenFileNames(parent, caption, dir, getExtensionsFromFilter(filter), options);
#else
    return QFileDialog::getOpenFileNames(parent, caption, dir, filter, selectedFilter, options);
#endif
}
