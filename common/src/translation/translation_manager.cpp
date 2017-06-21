#include "translation_manager.h"

#include <QtCore/QDir>
#include <QtCore/QTranslator>
#include <QtCore/QCoreApplication>

#include <utils/common/app_info.h>
#include <utils/common/warnings.h>

#include <nx/utils/log/assert.h>

namespace {

static const QString kDefaultSearchPath(lit(":/translations"));
static const QString kDefaultPrefix(lit("common"));
static const QString kTranslationPathPrefix = lit(":/translations/common_");
static const QString kTranslationPathExtension = lit(".qm");

// Handling compatibility with older clients.
static const QMap<QString, QString> kLocaleCodes30to31{
    {lit("ar"), lit("ar_SA")},
    {lit("de"), lit("de_DE")},
    {lit("es"), lit("es_ES")},
    {lit("fil"), lit("fil_PH")},
    {lit("fr"), lit("fr_FR")},
    {lit("he"), lit("he_IL")},
    {lit("hr"), lit("hr_HR")},
    {lit("hu"), lit("hu_HU")},
    {lit("id"), lit("id_ID")},
    {lit("ja"), lit("ja_JP")},
    {lit("km"), lit("km_KH")},
    {lit("ko"), lit("ko_KR")},
    {lit("pl"), lit("pl_PL")},
    {lit("ru"), lit("ru_RU")},
    {lit("th"), lit("th_TH")},
    {lit("tr"), lit("tr_TR")}
};

} // namespace

QnTranslationManager::QnTranslationManager(QObject *parent):
    QObject(parent),
    m_translationsValid(false)
{
    addPrefix(kDefaultPrefix);
    addPrefix(lit("qtbase"));
    addSearchPath(kDefaultSearchPath);
}

QnTranslationManager::~QnTranslationManager()
{
    return;
}

const QList<QString> &QnTranslationManager::prefixes() const
{
    return m_prefixes;
}

void QnTranslationManager::setPrefixes(const QList<QString> &prefixes)
{
    if (m_prefixes == prefixes)
        return;

    m_prefixes = prefixes;
    m_translationsValid = false;
}

void QnTranslationManager::addPrefix(const QString &prefix)
{
    if (m_prefixes.contains(prefix))
        return;

    m_prefixes.push_back(prefix);
    m_translationsValid = false;
}

void QnTranslationManager::removePrefix(const QString &prefix)
{
    if (!m_prefixes.contains(prefix))
        return;

    m_prefixes.removeOne(prefix);
    m_translationsValid = false;
}

const QList<QString> &QnTranslationManager::searchPaths() const
{
    return m_searchPaths;
}

void QnTranslationManager::setSearchPaths(const QList<QString> &searchPaths)
{
    if (m_searchPaths == searchPaths)
        return;

    m_searchPaths = searchPaths;
    m_translationsValid = false;
}

void QnTranslationManager::addSearchPath(const QString &searchPath)
{
    if (m_searchPaths.contains(searchPath))
        return;

    m_searchPaths.push_back(searchPath);
    m_translationsValid = false;
}

void QnTranslationManager::removeSearchPath(const QString &searchPath)
{
    if (!m_searchPaths.contains(searchPath))
        return;

    m_searchPaths.removeOne(searchPath);
    m_translationsValid = false;
}

void QnTranslationManager::installTranslation(const QnTranslation &translation)
{
    QString localeCode = translation.localeCode();
    localeCode.replace(L'-', L'_'); /* Minus sign is not supported as a separator... */

    QLocale locale(localeCode);
    if (locale.language() != QLocale::C)
        QLocale::setDefault(locale);

    for (const QString &file : translation.filePaths())
    {
        QScopedPointer<QTranslator> translator(new QTranslator(qApp));
        if (translator->load(file))
            qApp->installTranslator(translator.take());
    }
}

QString QnTranslationManager::localeCodeToTranslationPath(const QString& localeCode)
{
    return kTranslationPathPrefix + localeCode + kTranslationPathExtension;
}

QString QnTranslationManager::translationPathToLocaleCode(const QString& translationPath)
{
    if (!translationPath.startsWith(kTranslationPathPrefix)
        || !translationPath.endsWith(kTranslationPathExtension))
    {
        return QnAppInfo::defaultLanguage();
    }

    QString result = translationPath.mid(kTranslationPathPrefix.length());
    result.chop(kTranslationPathExtension.length());
    return result;
}

QString QnTranslationManager::localeCode31to30(const QString& localeCode)
{
    return kLocaleCodes30to31.key(localeCode, localeCode);
}

QString QnTranslationManager::localeCode30to31(const QString& localeCode)
{
    return kLocaleCodes30to31.value(localeCode, localeCode);
}

QnTranslation QnTranslationManager::defaultTranslation() const
{
    return loadTranslation(QnAppInfo::defaultLanguage());
}

QList<QnTranslation> QnTranslationManager::loadTranslations()
{
    if (!m_translationsValid)
    {
        m_translations = loadTranslationsInternal();
        m_translationsValid = true;
    }

    return m_translations;
}

QnTranslation QnTranslationManager::loadTranslation(const QString &locale) const
{
    if (m_prefixes.isEmpty())
        return QnTranslation();

    QFileInfo fileInfo(localeCodeToTranslationPath(locale));
    return loadTranslationInternal(fileInfo.dir().path(), fileInfo.fileName());
}

QList<QnTranslation> QnTranslationManager::loadTranslationsInternal() const
{
    QList<QnTranslation> result;

    if (m_searchPaths.isEmpty() || m_prefixes.isEmpty())
        return result;

    for (const QString& path: m_searchPaths)
    {
        QDir dir(path);
        if (!dir.exists())
            continue;

        QString mask = m_prefixes[0] + QLatin1String("*.qm");
        for (const QString& fileName : dir.entryList({mask}))
        {
            QnTranslation translation = loadTranslationInternal(dir.absolutePath(), fileName);
            if (!translation.isEmpty())
                result.push_back(translation);
        }
    }

    return result;
}

QnTranslation QnTranslationManager::loadTranslationInternal(
    const QString& translationDir,
    const QString& translationName) const
{
    QString filePath = translationDir + L'/' + translationName;
    QString suffix = translationName.mid(kDefaultPrefix.size());
    NX_ASSERT(suffix.endsWith(kTranslationPathExtension));
    NX_ASSERT(suffix.startsWith(L'_'));

    if (!QFileInfo(filePath).exists())
        return QnTranslation();

    QTranslator translator;
    if (!translator.load(filePath))
        return QnTranslation();

    // Suffix looks like '_en_US.qm'.
    const QString localeCode = suffix.mid(1, suffix.length() - kTranslationPathExtension.length() - 1);

    //: Language name that will be displayed to user. Must not be empty.
    QString languageName = translator.translate("Language", "Language Name");

    if (languageName.isEmpty() || localeCode.isEmpty())
        return QnTranslation(); //< Invalid translation.

    QStringList filePaths;
    for (int i = 0; i < m_prefixes.size(); i++)
        filePaths.push_back(translationDir + L'/' + m_prefixes[i] + suffix);

    return QnTranslation(languageName, localeCode, filePaths);
}
