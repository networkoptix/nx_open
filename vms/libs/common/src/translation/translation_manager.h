#pragma once

#include "translation.h"
#include "translation_overlay.h"

#include <chrono>

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QHash>
#include <QtCore/QStack>
#include <QtCore/QString>

#include <nx/utils/thread/mutex.h>

namespace nx::vms::translation {

class PreloadedTranslationReference;
class LocaleRollback;

} // namespace nx::vms::translation

class QnTranslationManager: public QObject
{
    Q_OBJECT

    friend nx::vms::translation::PreloadedTranslationReference;
    friend nx::vms::translation::LocaleRollback;

public:
    QnTranslationManager(QObject *parent = NULL);
    virtual ~QnTranslationManager();

    const QList<QString>& prefixes() const;
    void setPrefixes(const QList<QString>& prefixes);
    void addPrefix(const QString& prefix);
    void removePrefix(const QString& prefix);

    const QList<QString>& searchPaths() const;
    void setSearchPaths(const QList<QString>& searchPaths);
    void addSearchPath(const QString& searchPath);
    void removeSearchPath(const QString& searchPath);

    QnTranslation defaultTranslation() const;

    QList<QnTranslation> loadTranslations();
    QnTranslation loadTranslation(const QString& locale) const;

    static void installTranslation(const QnTranslation& translation);

    /**
     * Prepares and asynchronously installs the specific locale translation data.
     * Preloaded translation is required for correct LocaleRollback functioning.
     *
     * Reference counting is used internally, so the same locale translation may be preloaded
     * several times. This locale data will be unloaded eventually when all references are deleted.
     *
     * Returns an empty value in the case of failure (e.g. when the locale is not found).
     */
    nx::vms::translation::PreloadedTranslationReference preloadTranslation(const QString& locale);

    static QString localeCodeToTranslationPath(const QString& localeCode);
    static QString translationPathToLocaleCode(const QString& translationPath);
    static QString localeCode31to30(const QString& localeCode);
    static QString localeCode30to31(const QString& localeCode);

protected:
    QList<QnTranslation> loadTranslationsInternal() const;
    QnTranslation loadTranslationInternal(const QString& translationDir,
        const QString& translationName) const;

private:
    void addPreloadedTranslationReference(const QString& locale);
    void removePreloadedTranslationReference(const QString& locale);

    void setCurrentThreadTranslationLocale(
        const QString& locale,
        std::chrono::milliseconds maxWaitTime);
    QString getCurrentThreadTranslationLocale() const;

private:
    QList<QString> m_searchPaths;
    QList<QString> m_prefixes;
    QList<QnTranslation> m_translations;
    bool m_translationsValid;

    mutable nx::Mutex m_mutex;
    QHash<Qt::HANDLE, QString> m_threadLocales;
    QHash<QString, QSharedPointer<nx::vms::translation::TranslationOverlay>> m_overlays;
};
