#pragma once

#include <chrono>

#include <translation/preloaded_translation_reference.h>

class QnTranslationManager;

namespace nx::vms::translation {

using namespace std::chrono;

/**
 * Temporary changes the locale that is used in the current thread to translate tr() strings.
 * When this object is destroyed, the original locale is restored.
 *
 * If target locale is invalid, default app language is used.
 */
class LocaleRollback
{
public:
    /**
     * Changes translation locale used in the current thread to the preloaded one.
     *
     * Could wait up to maxWaitTime if the translation hasn't been installed yet,
     * however preloaded translation usage assumes that it shoudn't be necessary.
     * See overloaded method description for details.
     */
    LocaleRollback(const PreloadedTranslationReference& target, milliseconds maxWaitTime = 0ms);

    /**
     * Changes translation locale used in the current thread. Translation is loaded if necessary.
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
     * for heavy loaded systems. It is a caller choise to set maximum waiting time that's
     * acceptable in their scenario.
     */
    LocaleRollback(QnTranslationManager* manager, const QString& locale, milliseconds maxWaitTime);

    /** Restores previous translation locale used in the current thread. */
    ~LocaleRollback();

private:
    QString m_prevLocale;
    QPointer<QnTranslationManager> m_manager;
    PreloadedTranslationReference m_translationRef;
};

} // namespace nx::vms::translation
