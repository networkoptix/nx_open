// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/utils/impl_ptr.h>

#include "translation.h"

namespace nx::i18n {

using namespace std::chrono_literals;

class PreloadedTranslationReference;
class ScopedLocale;

/**
 * Loads all translation files from the external resources. Provides interface to install
 * translation on an application level as well as separately for each thread.
 * Default QLocale is also updated when application-wide translation is set.
 */
class NX_UTILS_API TranslationManager: public QObject
{
    friend ScopedLocale;
    friend PreloadedTranslationReference;

public:
    TranslationManager(QObject* parent = nullptr);
    virtual ~TranslationManager() override;

    /** List of all existing translations. */
    QList<Translation> translations() const;

    /**
     * Find a translation by it's locale code and install it on an application level.
     * @return True if the translation was found, false otherwise.
     */
    bool installTranslation(const QString& localeCode);

    /** Install the provided translation on an application level. */
    void installTranslation(const Translation& translation);

    static std::chrono::milliseconds defaultInstallTranslationTimeout();

    /*
     * Prepare and asynchronously install the specific locale translation data.
     * Reference counting is used internally, so the same locale translation may be preloaded
     * several times. This locale data will be unloaded eventually when all references are deleted.
     * @return Preloaded translation reference, or an empty reference if the locale was not found.
     */
    [[nodiscard]]
    PreloadedTranslationReference preloadTranslation(const QString& locale);

    /**
     * Temporary change the Qt-specific locale in the current thread, so all translatable strings
     * will use it. When this object is destroyed, the original locale is restored.
     * This method could wait up to maxWaitTime if the translation hasn't been installed yet,
     * however preloaded translation usage assumes that it shouldn't be necessary.
     * @return Scoped guard object.
     */
    [[nodiscard]]
    std::unique_ptr<ScopedLocale> installScopedLocale(
        const PreloadedTranslationReference& locale,
        std::chrono::milliseconds maxWaitTime = 0ms);

    /**
     * Overloaded method.
     *
     * If the target locale has not been installed, it tries to install it in the main thread and
     * waits up to maxWaitTime. If installation isn't finished after that, the app default language
     * is used (so the target locale wouldn't be used even if translators installation is completed
     * during rollback lifetime, meaning that all strings translated under the rollback are
     * consistent and are translated into a single language).
     *
     * The target translation can be installed only from the app main thread due to Qt limitations,
     * so in some circumstances installation may hang because the main thread awaits a mutex locked
     * somewhere up the stack of the current thread. In this case locale wouldn't be loaded,
     * whatever large waiting time is used. In other cases quite large waiting time may be helpful
     * for heavy loaded systems.
     *
     * If locale is not found, default app language would be used.
     */
    [[nodiscard]]
    std::unique_ptr<ScopedLocale> installScopedLocale(
        const QString& locale,
        std::chrono::milliseconds maxWaitTime = defaultInstallTranslationTimeout());

    /**
     * Overloaded method.
     *
     * All passed locales are checked one-by-one. First found is applied. If no locales found,
     * default app language is used.
     */
    [[nodiscard]]
    std::unique_ptr<ScopedLocale> installScopedLocale(
        const std::vector<QString>& preferredLocales,
        std::chrono::milliseconds maxWaitTime = defaultInstallTranslationTimeout());

    /** Stop translation manager from loading new translations. */
    void stopLoadingTranslations();

    /** Remove all translators. */
    void deinitialize();

    /**
     * When assert on failure is enabled, TranslationManager will report an error (assert) if
     * installScopedLocale() call was not executed within the given timelimit. Such behavior could
     * be helpful to detect unwanted locks of the main app thread. By default, assert is disabled
     * since some integration tests could lock the main thread until the testing action is done.
     */
    void setAssertOnOverlayInstallationFailure(bool assert = true);

    static QString getCurrentThreadLocale();

protected:
    static void setCurrentThreadLocale(QString locale);
    static void eraseCurrentThreadLocale();

private:
    /**
     * Find a translation by its locale code and install it on a thread level. If empty locale is
     * passed, then current thread overlay is cleared, and application-wide locale will be used in
     * this thread further.
     * Translation installation may require actions in the app main thread and may lead to deadlock
     * in some circumstances. Therefore this method waits only up to maxWaitTime and returns even
     * if installation is not completed.
     * @return True on success, false on failure.
     */
    bool setCurrentThreadTranslationLocale(
        const QString& locale,
        std::chrono::milliseconds maxWaitTime);

    void addPreloadedTranslationReference(const QString& locale);
    void removePreloadedTranslationReference(const QString& locale);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::i18n
