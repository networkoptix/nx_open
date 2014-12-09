#include "translation_manager.h"

#include <QtCore/QDir>
#include <QtCore/QTranslator>
#include <QtCore/QCoreApplication>

#include <utils/common/warnings.h>
#include <common/common_globals.h>

QnTranslationManager::QnTranslationManager(QObject *parent):
    QObject(parent),
    m_translationsValid(false)
{
    addPrefix(lit("common"));
    addPrefix(lit("qt"));
    addPrefix(lit("qtbase"));
    addPrefix(lit("qtmultimedia"));
    
    addSearchPath(lit(":/translations"));
    // Closing a backdoor for custom translations --Elric
    /*if(qApp) {
        addSearchPath(qApp->applicationDirPath() + lit("/translations"));
    } else {
        qnWarning("QApplication instance does not exist, executable-relative translations will not be loaded.");
    }*/
}

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

void QnTranslationManager::addPrefix(const QString &prefix) {
    if(m_prefixes.contains(prefix))
        return;

    m_prefixes.push_back(prefix);
    m_translationsValid = false;
}

void QnTranslationManager::removePrefix(const QString &prefix) {
    if(!m_prefixes.contains(prefix))
        return;

    m_prefixes.removeOne(prefix);
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

void QnTranslationManager::addSearchPath(const QString &searchPath) {
    if(m_searchPaths.contains(searchPath))
        return;

    m_searchPaths.push_back(searchPath);
    m_translationsValid = false;
}

void QnTranslationManager::removeSearchPath(const QString &searchPath) {
    if(!m_searchPaths.contains(searchPath))
        return;

    m_searchPaths.removeOne(searchPath);
    m_translationsValid = false;
}

void QnTranslationManager::installTranslation(const QnTranslation &translation) {
    QString localeCode = translation.localeCode();
    localeCode.replace(L'-', L'_'); /* Minus sign is not supported as a separator... */

    QLocale locale(localeCode);
    if(locale.language() != QLocale::C)
        QLocale::setDefault(locale);

    for(const QString &file: translation.filePaths()) {
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

    for(const QString &path: m_searchPaths) {
        QDir dir(path);
        if(!dir.exists())
            continue;

        QString mask = m_prefixes[0] + lit("*.qm");
        for(const QString &fileName: dir.entryList(QStringList(mask))) {
            QnTranslation translation = loadTranslationInternal(dir.absolutePath(), fileName);
            if(!translation.isEmpty())
                result.push_back(translation);
        }
    }

    return result;
}

QnTranslation QnTranslationManager::loadTranslationInternal(const QString &translationDir, const QString &translationName) const {
    QString filePath = translationDir + L'/' + translationName;
    QString suffix = translationName.mid(m_prefixes[0].size());

    QTranslator translator;
    translator.load(filePath);

    /* Note that '//:' denotes a comment for translators that will appear in TS files. */

    //: Language name that will be displayed to user. Must not be empty.
    QString languageName = translator.translate("Language", "Language Name");

    //: Internal. Please don't change existing translation.
    QString localeCode = translator.translate("Language", "Locale Code");

    if(languageName.isEmpty() || localeCode.isEmpty())
        return QnTranslation(); /* Invalid translation. */

    QStringList filePaths;
    for(int i = 0; i < m_prefixes.size(); i++)
        filePaths.push_back(translationDir + L'/' + m_prefixes[i] + suffix);

    return QnTranslation(languageName, localeCode, filePaths);
}

