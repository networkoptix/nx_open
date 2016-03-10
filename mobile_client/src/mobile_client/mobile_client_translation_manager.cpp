#include "mobile_client_translation_manager.h"

#include <QtCore/QLocale>

QnMobileClientTranslationManager::QnMobileClientTranslationManager(QObject *parent) :
    base_type(parent)
{
    addPrefix(lit("mobile_client_base"));
    addPrefix(lit("mobile_client_qml"));
}

QnMobileClientTranslationManager::~QnMobileClientTranslationManager()
{
}

void QnMobileClientTranslationManager::updateTranslation()
{
    QString localeName = QLocale::system().name();
    QString langName = localeName.split(L'_').first();

    QnTranslation bestTranslation;

    for (const QnTranslation &translation: loadTranslations()) {
        if (translation.localeCode() == localeName) {
            bestTranslation = translation;
            break;
        }
        if (translation.localeCode() == langName) {
            bestTranslation = translation;
            // continue to find the exact translation
        }
    }

    if (!bestTranslation.isEmpty()) {
        qDebug() << "Installing translation: " << bestTranslation.localeCode();
        installTranslation(bestTranslation);
    }
}
