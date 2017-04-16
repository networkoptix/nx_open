#include "ui/dialogs/common/file_dialog.h"

#include <QtCore/QSet>

#ifdef Q_OS_MAC
#include "ui/workaround/mac_utils.h"
#endif

namespace {

#if defined(Q_OS_MAC)
    QStringList getExtensionsFromFilter(const QString &filter) {
        QRegExp filterRegExp(QLatin1String(".*\\((.*)\\).*"));
        QRegExp extensionRegExp(QLatin1String("\\*\\.(.*)"));

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
#endif // defined(Q_OS_MAC)

}

QString QnFileDialog::getExistingDirectory(QWidget *parent,
                                        const QString &caption,
                                        const QString &dir,
                                        Options options) {
#ifdef Q_OS_MAC
    Q_UNUSED(parent)
    Q_UNUSED(options)
    return mac_getExistingDirectory(caption, dir);
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
    Q_UNUSED(parent)
    Q_UNUSED(options)
    return mac_getOpenFileName(caption, dir, getExtensionsFromFilter(filter));
#else
    return QFileDialog::getOpenFileName(parent, caption, dir, filter, selectedFilter, options);
#endif
}

QString QnFileDialog::getSaveFileName(QWidget *parent, const QString &caption, const QString &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options)
{
#ifdef Q_OS_MAC
    Q_UNUSED(parent)
    Q_UNUSED(options)
    return mac_getSaveFileName(caption, dir, getExtensionsFromFilter(filter));
#else
    return QFileDialog::getSaveFileName(parent, caption, dir, filter, selectedFilter, options);
#endif
}

QStringList QnFileDialog::getOpenFileNames(QWidget *parent, const QString &caption, const QString &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options)
{
#ifdef Q_OS_MAC
    Q_UNUSED(parent)
    Q_UNUSED(options)
    return mac_getOpenFileNames(caption, dir, getExtensionsFromFilter(filter));
#else
    return QFileDialog::getOpenFileNames(parent, caption, dir, filter, selectedFilter, options);
#endif
}
