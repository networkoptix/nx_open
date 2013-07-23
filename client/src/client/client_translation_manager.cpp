#include "client_translation_manager.h"

#include <client/client_settings.h>

QnClientTranslationManager::QnClientTranslationManager(QObject *parent):
    base_type(parent)
{
    addPrefix(lit("client"));

    QString extraPath = qnSettings->extraTranslationsPath();
    if(!extraPath.isEmpty())
        addSearchPath(extraPath);
}

QnClientTranslationManager::~QnClientTranslationManager() {
    return;
}

