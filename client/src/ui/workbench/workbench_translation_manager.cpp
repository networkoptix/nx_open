#include "workbench_translation_manager.h"

#include <QtCore/QDir>
#include <QtCore/QTranslator>
#include <QtGui/QApplication>

#include "utils/settings.h"

QnWorkbenchTranslationManager::QnWorkbenchTranslationManager(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_translationsValid(false)
{}

QnWorkbenchTranslationManager::~QnWorkbenchTranslationManager() {
    return;
}

const QList<QnTranslationInfo> &QnWorkbenchTranslationManager::translations() const {
    ensureTranslations();

    return m_translations;
}

void QnWorkbenchTranslationManager::installTranslation(const QString &translationPath) {
    QString localeCode;

    QTranslator *clientTranslator = new QTranslator(qApp);
    if(clientTranslator->load(translationPath)) {
        qApp->installTranslator(clientTranslator);

        localeCode = clientTranslator->translate("Language", "Locale Code");
    }

    int index = translationPath.lastIndexOf(QLatin1String("client"));
    if(index != -1) {
        QString commonTranslationPath = translationPath;
        commonTranslationPath.replace(index, 6, QLatin1String("common"));
        
        QTranslator *commonTranslator = new QTranslator(qApp);
        if (commonTranslator->load(commonTranslationPath))
            qApp->installTranslator(commonTranslator);

        QString qtTranslationPath = translationPath;
        qtTranslationPath.replace(index, 6, QLatin1String("qt"));

        QTranslator *qtTranslator = new QTranslator(qApp);
        if (qtTranslator->load(qtTranslationPath))
            qApp->installTranslator(qtTranslator);
    }
}

void QnWorkbenchTranslationManager::ensureTranslations() const {
    if(m_translationsValid)
        return;

    QList<QnTranslationInfo> translations;

    QList<QString> paths;
    paths.push_back(QLatin1String(":/translations"));
    paths.push_back(QApplication::applicationDirPath() + QLatin1String("/translations"));
    
    QString extraPath = qnSettings->extraTranslationsPath();
    if(!extraPath.isEmpty())
        paths.push_back(extraPath);

    foreach(const QString &path, paths) {
        QDir dir(path);
        if(!dir.exists())
            continue;

        foreach(const QString &translationFile, dir.entryList(QStringList(QLatin1String("client*.qm")))) {
            QString translationPath = dir.absolutePath() + QLatin1Char('/') + translationFile;

            QTranslator translator;
            translator.load(translationPath);
            QString languageName = translator.translate("Language", "Language Name");
            QString localeCode = translator.translate("Language", "Locale Code");
            if(languageName.isEmpty() || localeCode.isEmpty())
                continue; /* Invalid translation. */

            translations.push_back(QnTranslationInfo(languageName, localeCode, translationPath));
        }
    }

    m_translations = translations;
    m_translationsValid = true;
}

