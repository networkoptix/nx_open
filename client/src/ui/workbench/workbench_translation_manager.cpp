#include "workbench_translation_manager.h"

#include <QtGui/QApplication>

#include <client/client_settings.h>

QnWorkbenchTranslationManager::QnWorkbenchTranslationManager(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    addPrefix(lit("client"));

    QString extraPath = qnSettings->extraTranslationsPath();
    if(!extraPath.isEmpty())
        addSearchPath(extraPath);
}

QnWorkbenchTranslationManager::~QnWorkbenchTranslationManager() {
    return;
}

