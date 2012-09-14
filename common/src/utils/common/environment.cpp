#include "environment.h"

#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtGui/QMessageBox>

QString QnEnvironment::searchInPath(QString executable) {
    if (executable.isEmpty())
        return QString();

    QFileInfo fi(executable);
    if (fi.isAbsolute() && fi.exists())
        return executable;

#ifdef Q_OS_WIN
    if (!executable.endsWith(QLatin1String(".exe")))
        executable.append(QLatin1String(".exe"));
#endif

#ifdef Q_OS_WIN
    const QChar sep = QLatin1Char(';');
#else
    const QChar sep = QLatin1Char(':');
#endif

    const QChar slash = QLatin1Char('/');
    foreach (const QString &p, QProcessEnvironment::systemEnvironment().value(QLatin1String("PATH")).split(sep)) {
        QString fp = p + slash + executable;

        const QFileInfo fi(fp);
        if (fi.exists())
            return fi.absoluteFilePath();
    }

    return QString();
}

void QnEnvironment::showInGraphicalShell(QWidget *parent, const QString &path) {
#if defined(Q_OS_WIN)
    const QString explorer = searchInPath(QLatin1String("explorer.exe"));
    if (explorer.isEmpty()) {
        QMessageBox::warning(parent,
            tr("Launching Windows Explorer failed"),
            tr("Could not find explorer.exe in path to launch Windows Explorer."));
        return;
    }
    
    QStringList params;
    if (!QFileInfo(path).isDir())
        params << QLatin1String("/select,");
    params << QDir::toNativeSeparators(path);
    QProcess::startDetached(explorer, params);
#elif defined(Q_OS_MAC)
    Q_UNUSED(parent);
    
    QStringList scriptArgs;
    scriptArgs 
        << QLatin1String("-e")
        << QString::fromLatin1("tell application \"Finder\" to reveal POSIX file \"%1\"").arg(path);
    QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
    
    scriptArgs.clear();
    scriptArgs 
        << QLatin1String("-e")
        << QLatin1String("tell application \"Finder\" to activate");
    QProcess::execute("/usr/bin/osascript", scriptArgs);
#else
    Q_UNUSED(parent)
    Q_UNUSED(path)
    /* We cannot select a file here, because no file browser really supports it... */
    // TODO: implement as in Qt Creator.
#endif
}

