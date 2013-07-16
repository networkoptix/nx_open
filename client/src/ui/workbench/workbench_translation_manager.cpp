#include "workbench_translation_manager.h"

#include <QtWidgets/QApplication>

#include <client/client_settings.h>

QnWorkbenchTranslationManager::QnWorkbenchTranslationManager(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    QList<QString> prefixes;
    prefixes.push_back(lit("client"));
    prefixes.push_back(lit("common"));
    prefixes.push_back(lit("qt"));

    QList<QString> searchPaths;
    searchPaths.push_back(QLatin1String(":/translations"));
    searchPaths.push_back(QApplication::applicationDirPath() + QLatin1String("/translations"));

    QString extraPath = qnSettings->extraTranslationsPath();
    if(!extraPath.isEmpty())
        searchPaths.push_back(extraPath);

    setPrefixes(prefixes);
    setSearchPaths(searchPaths);
}

QnWorkbenchTranslationManager::~QnWorkbenchTranslationManager() {
    return;
}

