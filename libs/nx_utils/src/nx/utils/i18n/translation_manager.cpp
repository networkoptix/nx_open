// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "translation_manager.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QThread>
#include <QtCore/QTranslator>

#include <nx/build_info.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/external_resources.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "preloaded_translation_reference.h"
#include "scoped_locale.h"
#include "translation_overlay.h"

namespace {

static const QString kSearchPath(":/translations");
thread_local QString sCurrentThreadLocale;

} // namespace

namespace nx::i18n {

struct TranslationManager::Private
{
    const QList<Translation> translations{loadTranslations()};
    QList<QPointer<QTranslator>> translators;

    mutable nx::Mutex mutex;
    QHash<QString, QSharedPointer<TranslationOverlay>> overlays;
    std::atomic<bool> assertOnOverlayInstallationFailure = false;
    std::atomic<bool> isLoadTranslationsEnabled = true;
    std::atomic<bool> initialized = true;

    std::optional<Translation> findTranslation(const QString& localeCode)
    {
        auto iter = std::find_if(translations.cbegin(), translations.cend(),
            [&](const auto& translation) { return translation.localeCode == localeCode; });

        if (iter == translations.cend())
            return std::nullopt;

        return *iter;
    }

private:
    Translation loadTranslation(const QString &locale)
    {
        Translation result;
        result.localeCode = locale;

        const QDir localePath(kSearchPath + "/" + locale);
        NX_ASSERT(localePath.exists(), "Translation %1 could not be loaded", locale);

        for (const QString& fileName: localePath.entryList(QDir::Files))
            result.filePaths.append(localePath.absoluteFilePath(fileName));

        NX_DEBUG(this, "Loaded translation: %1", result);
        return result;
    }

    QList<Translation> loadTranslations()
    {
        utils::registerExternalResourceDirectory("translations");
        QList<Translation> result;

        QDir dir(kSearchPath);
        NX_ASSERT(dir.exists(), "Translations could not be loaded from resources: %1", kSearchPath);

        for (const QString& locale: dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            result.push_back(loadTranslation(locale));

        NX_INFO(this, "Loaded %1 translations from %2", result.size(), kSearchPath);
        return result;
    }
};

TranslationManager::TranslationManager(QObject *parent):
    QObject(parent),
    d(new Private())
{
}

TranslationManager::~TranslationManager()
{
    deinitialize();
}

QString TranslationManager::getCurrentThreadLocale()
{
    return sCurrentThreadLocale;
}

void TranslationManager::setCurrentThreadLocale(QString locale)
{
    sCurrentThreadLocale = locale;
}

void TranslationManager::eraseCurrentThreadLocale()
{
    sCurrentThreadLocale = {};
}

QList<Translation> TranslationManager::translations() const
{
    return d->translations;
}

bool TranslationManager::installTranslation(const QString& localeCode)
{
    auto translation = d->findTranslation(localeCode);
    if (!translation)
        return false;

    installTranslation(*translation);
    return true;
}

void TranslationManager::installTranslation(const Translation& translation)
{
    // Translations in the server can be installed in a delayed functor, after the manager was
    // deinitialized already.
    if (!d->isLoadTranslationsEnabled)
        return;

    QString localeCode = translation.localeCode;
    localeCode.replace('-', '_'); /* Minus sign is not supported as a separator... */

    QLocale locale(localeCode);
    if (locale.language() != QLocale::C)
        QLocale::setDefault(locale);

    for (const QString& file: translation.filePaths)
    {
        auto translator = new QTranslator(qApp);
        if (NX_ASSERT(translator->load(file), "File %1 cannot be loaded", file))
        {
            d->translators.append(translator);
            qApp->installTranslator(translator);
        }
        else
        {
            delete translator;
        }
    }
}

std::chrono::milliseconds TranslationManager::defaultInstallTranslationTimeout()
{
    static const std::chrono::seconds value([] { return nx::build_info::isArm() ? 5 : 3; }());
    return value;
}

PreloadedTranslationReference TranslationManager::preloadTranslation(
    const QString& locale)
{
    {
        NX_MUTEX_LOCKER lock(&d->mutex);

        if (!d->overlays.contains(locale))
        {
            // Load the required translation.
            auto translation = d->findTranslation(locale);

            if (!translation)
            {
                // Write to log, return an empty reference.
                NX_WARNING(this, "Could not load translation for locale '%1'", locale);
                return PreloadedTranslationReference(this, {});
            }

            // Create translators.
            d->overlays[locale] = QSharedPointer<TranslationOverlay>(
                new TranslationOverlay(std::move(*translation)));
        }
    }

    return PreloadedTranslationReference(this, locale);
}

std::unique_ptr<ScopedLocale> TranslationManager::installScopedLocale(
    const PreloadedTranslationReference& locale,
    std::chrono::milliseconds maxWaitTime)
{
    return std::make_unique<ScopedLocale>(locale, maxWaitTime);
}

std::unique_ptr<ScopedLocale> TranslationManager::installScopedLocale(
    const QString& locale,
    std::chrono::milliseconds maxWaitTime)
{
    return installScopedLocale(std::vector<QString>{locale}, maxWaitTime);
}

std::unique_ptr<ScopedLocale> TranslationManager::installScopedLocale(
    const std::vector<QString>& preferredLocales,
    std::chrono::milliseconds maxWaitTime)
{
    nx::utils::ElapsedTimer timer(nx::utils::ElapsedTimerState::started);

    if (!d->isLoadTranslationsEnabled)
        return std::make_unique<ScopedLocale>(PreloadedTranslationReference(), 0ms);

    for (const auto& locale: preferredLocales)
    {
        // Load translation data.
        const auto translationRef = preloadTranslation(locale);
        if (!translationRef.locale().isEmpty())
        {
            static const auto kMinInstallationTime = 100ms;
            // It's possible that translation loading itself would take more than maxWaitingTime.
            // In this case lets spend some more time trying to install the new locale, since we've already
            // failed to fit into the given interval.
            return std::make_unique<ScopedLocale>(
                translationRef,
                qMax(maxWaitTime - timer.elapsed(), kMinInstallationTime));
        }
    }

    // Can't find a valid translation locale. Switch to default app language.
    return std::make_unique<ScopedLocale>(PreloadedTranslationReference(), 0ms);
}

void TranslationManager::stopLoadingTranslations()
{
    d->isLoadTranslationsEnabled = false;
}

void TranslationManager::deinitialize()
{
    stopLoadingTranslations();
    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        if (!d->initialized) //< Avoid double deinitialization.
            return;

        for (auto translator: d->translators)
        {
            if (translator)
                qApp->removeTranslator(translator);
            delete translator.get();
        }
        d->translators.clear();

        for (const auto& overlay: d->overlays.values())
            NX_ASSERT(!overlay->isUsed());
        d->overlays.clear();
        d->initialized = false;
    }

    // The directory was registered in loadTranslations() method.
    utils::unregisterExternalResourceDirectory("translations");
}

void TranslationManager::setAssertOnOverlayInstallationFailure(bool assert)
{
    d->assertOnOverlayInstallationFailure = assert;
}

bool TranslationManager::setCurrentThreadTranslationLocale(
    const QString& locale,
    std::chrono::milliseconds maxWaitTime)
{
    NX_MUTEX_LOCKER lock(&d->mutex);
    const auto curLocale = getCurrentThreadLocale();

    if (locale == curLocale)
        return true;

    Qt::HANDLE id = QThread::currentThreadId();
    if (!curLocale.isEmpty())
    {
        // We have overlayed locale for the given thread already. Disable the existing translators.
        d->overlays[curLocale]->removeThreadContext(id);
    }

    if (!locale.isEmpty())
    {
        if (NX_ASSERT(d->overlays.contains(locale), "Locale '%1' has not been loaded", locale))
        {
            auto& overlay = d->overlays[locale];

            if (maxWaitTime.count() > 0)
                overlay->waitForInstallation(maxWaitTime);

            const bool overlayIsInstalled = overlay->isInstalled();
            // Some "unit" tests are actually integration tests that sleep in the main thread
            // waiting until some activity (e.g. interaction with a cloud) is completed. Since
            // the main thread is locked, translation overlay can't be installed in such test
            // cases, therefore installation failure could be expected here.
            if (d->assertOnOverlayInstallationFailure)
            {
                NX_ASSERT(overlayIsInstalled,
                    "Translation is not installed for locale '%1' within %2", locale, maxWaitTime);
            }
            if (overlayIsInstalled)
            {
                // Enable translation overlay for the given thread.
                overlay->addThreadContext(id);

                // Store the new locale of the thread.
                setCurrentThreadLocale(locale);

                // Done.
                return true;
            }
        }

        // Locale doesn't exist or can't be installed in time. Switch to the app default language.
        eraseCurrentThreadLocale();
        return false;
    }

    // An empty locale. Switch to the app default language.
    eraseCurrentThreadLocale();
    return true;
}

void TranslationManager::addPreloadedTranslationReference(const QString& locale)
{
    if (locale.isEmpty())
        return;

    NX_MUTEX_LOCKER lock(&d->mutex);

    if (!NX_ASSERT(d->overlays.contains(locale), "Locale '%1' has not been loaded", locale))
        return;

    d->overlays[locale]->addRef();
}

void TranslationManager::removePreloadedTranslationReference(const QString& locale)
{
    if (locale.isEmpty())
        return;

    NX_MUTEX_LOCKER lock(&d->mutex);

    if (!NX_ASSERT(d->overlays.contains(locale), "Locale '%1' has not been loaded", locale))
        return;

    d->overlays[locale]->removeRef();
    // Dereferenced translation data is not deleted, so it may be reused later.
}

} // namespace nx::i18n
