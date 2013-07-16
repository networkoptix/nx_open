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
    foreach(const QString &file, translation.filePaths()) {
        QScopedPointer<QTranslator> translator(new QTranslator(qApp));
        if(translator->load(file))
            qApp->installTranslator(translator.take());
    }
}

QList<QnTranslation> QnTranslationManager::loadTranslations() {
    if(!m_translationsValid) {
        m_translations = loadTranslationsInternal();
        m_translationsValid = true;
    }

    return m_translations;
}

QnTranslation QnTranslationManager::loadTranslation(const QString &translationPath) {
    if(m_prefixes.isEmpty())
        return QnTranslation();

    QFileInfo fileInfo(translationPath);

    return loadTranslationInternal(fileInfo.dir().path(), fileInfo.fileName());
}

QList<QnTranslation> QnTranslationManager::loadTranslationsInternal() const {
    QList<QnTranslation> result;

    if(m_searchPaths.isEmpty() || m_prefixes.isEmpty())
        return result;

    foreach(const QString &path, m_searchPaths) {
        QDir dir(path);
        if(!dir.exists())
            continue;

        foreach(const QString &fileName, dir.entryList(QStringList(m_prefixes[0] + lit("*.qm")))) {
            QnTranslation translation = loadTranslationInternal(dir.absolutePath(), fileName);
            if(!translation.isEmpty())
                result.push_back(translation);
        }
    }

    return result;
}

QnTranslation QnTranslationManager::loadTranslationInternal(const QString &translationDir, const QString &translationName) const {
    QString filePath = translationDir + lit('/') + translationName;
    QString suffix = translationName.mid(m_prefixes[0].size());

    QTranslator translator;
    translator.load(filePath);
    QString languageName = translator.translate("Language", "Language Name");
    QString localeCode = translator.translate("Language", "Locale Code");
    if(languageName.isEmpty() || localeCode.isEmpty())
        return QnTranslation(); /* Invalid translation. */

    QStringList filePaths;
    for(int i = 0; i < m_prefixes.size(); i++)
        filePaths.push_back(translationDir + lit('/') + m_prefixes[i] + suffix);

    return QnTranslation(languageName, localeCode, suffix, filePaths);
}

