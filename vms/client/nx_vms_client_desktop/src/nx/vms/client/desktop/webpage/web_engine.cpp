// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "web_engine.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineSettings>

#include <nx/build_info.h>

namespace nx::vms::client::desktop {

void initWebEngine()
{
    // QtWebEngine uses a dedicated process to handle web pages. That process needs to know from
    // where to load libraries. It's not a problem for release packages since everything is
    // gathered in one place, but it is a problem for development builds. The simplest solution for
    // this is to set library search path variable. In Linux this variable is needed only for
    // QtWebEngine::defaultSettings() call which seems to create a WebEngine process. After the
    // variable could be restored to the original value. In macOS it's needed for every web page
    // constructor, so we just set it for the whole lifetime of Client application.

    const QByteArray libraryPathVariable =
        nx::build_info::isLinux()
            ? "LD_LIBRARY_PATH"
            : nx::build_info::isMacOsX()
                ? "DYLD_LIBRARY_PATH"
                : "";

    const QByteArray originalLibraryPath =
        libraryPathVariable.isEmpty() ? QByteArray() : qgetenv(libraryPathVariable);

    if (!libraryPathVariable.isEmpty())
    {
        QString libPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../lib");
        if (!originalLibraryPath.isEmpty())
        {
            libPath += ':';
            libPath += originalLibraryPath;
        }

        qputenv(libraryPathVariable, libPath.toLocal8Bit());
    }

    qputenv("QTWEBENGINE_DIALOG_SET", "QtQuickControls2");

    const auto settings = QWebEngineProfile::defaultProfile()->settings();
    // We must support Flash for some camera admin pages to work.
    settings->setAttribute(QWebEngineSettings::PluginsEnabled, true);

    // TODO: Add ini parameters for WebEngine attributes
    //settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);
    //settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);

    if (!nx::build_info::isMacOsX())
    {
        if (!originalLibraryPath.isEmpty())
            qputenv(libraryPathVariable, originalLibraryPath);
        else
            qunsetenv(libraryPathVariable);
    }
}

} // namespace nx::vms::client::desktop
