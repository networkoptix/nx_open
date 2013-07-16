#include "translation_manager.h"

#include <QtCore/QDir>
#include <QtCore/QTranslator>
#include <QtGui/QApplication>

QnTranslationManager::QnTranslationManager(QObject *parent):
    QObject(parent),
    m_translationsValid(false)
{}

QnTranslationManager::~QnTranslationManager() {
    return;
}

const QList<QString> &QnTranslationManager::prefixes() const {
    return m_prefixes;
}

void QnTranslationManager::setPrefixes(const QList<QString> &prefixes) {
    if(m_prefixes == prefixes)
        return;

    m_prefixes = prefixes;
    m_translationsValid = false;
}

const QList<QString> &QnTranslationManager::searchPaths() const {
    return m_searchPaths;
}

void QnTranslationManager::setSearchPaths(const QList<QString> &searchPaths) {
    if(m_searchPaths == searchPaths)
        return;

    m_searchPaths = searchPaths;
    m_translationsValid = false;
}

void QnTranslationManager::installTranslation(const QnTranslation &translation) {
    return installTranslation(translation.translationPath);
}

void QnTranslationManager::installTranslation(const QString &translationPath) {
    QScopedPointer<QTranslator> translator(new QTranslator(qApp));
    if(translator->load(translationPath))
        qApp->installTranslator(translator.take());

    if(m_prefixes.isEmpty())
        return;
    
    int index = translationPath.lastIndexOf(m_prefixes[0]);
    if(index == -1)
        return;

    for(int i = 1; i < m_prefixes.size(); i++) {
        QString secondaryTranslationPath = translationPath;
        secondaryTranslationPath.replace(index, m_prefixes[0].size(), m_prefixes[i]);

        QScopedPointer<QTranslator> secondaryTranslator(new QTranslator(qApp));
        if (secondaryTranslator->load(secondaryTranslationPath))
            qApp->installTranslator(secondaryTranslator.take());
    }
}

QList<QnTranslation> QnTranslationManager::loadTranslations() {
    if(!m_translationsValid) {
        m_translations = loadTranslations();
        m_translationsValid = true;
    }

    return m_translations;
}

QList<QnTranslation> QnTranslationManager::loadTranslationsInternal() const {
    QList<QnTranslation> result;

    if(m_searchPaths.isEmpty() || m_prefixes.isEmpty())
        return result;

    foreach(const QString &path, m_searchPaths) {
        QDir dir(path);
        if(!dir.exists())
            continue;

        foreach(const QString &translationFile, dir.entryList(QStringList(m_prefixes[0] + lit("*.qm")))) {
            QString translationPath = dir.absolutePath() + QLatin1Char('/') + translationFile;

            QTranslator translator;
            translator.load(translationPath);
            QString languageName = translator.translate("Language", "Language Name");
            QString localeCode = translator.translate("Language", "Locale Code");
            if(languageName.isEmpty() || localeCode.isEmpty())
                continue; /* Invalid translation. */

            result.push_back(QnTranslation(languageName, localeCode, translationPath));
        }
    }

    return result;
}

