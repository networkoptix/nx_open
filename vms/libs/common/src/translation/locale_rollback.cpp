#include "locale_rollback.h"

#include <QtCore/QTime>

#include <nx/utils/elapsed_timer.h>
#include <translation/translation_manager.h>

namespace nx::vms::translation {

LocaleRollback::LocaleRollback(
    const PreloadedTranslationReference& target,
    milliseconds maxWaitTime)
    :
    m_manager(target.manager())
{
    if (!m_manager)
        return;

    m_prevLocale = m_manager->getCurrentThreadTranslationLocale();
    m_manager->setCurrentThreadTranslationLocale(target.locale(), maxWaitTime);
}

LocaleRollback::LocaleRollback(
    QnTranslationManager* manager,
    const QString& locale,
    milliseconds maxWaitTime)
    :
    m_manager(manager)
{
    if (!m_manager)
        return;

    nx::utils::ElapsedTimer timer(/*started*/ true);
    m_prevLocale = m_manager->getCurrentThreadTranslationLocale();

    // Load translation data. Save the reference, so it would be alive until rollback is deleted.
    m_translationRef = m_manager->preloadTranslation(locale);

    // It's possible that translation loading itself would take more than maxWaitingTime.
    // In this case lets spend some more time trying to install the new locale, since we've already
    // failed to fit into the given interval.
    static const auto kMinInstallationTime = 100ms;
    m_manager->setCurrentThreadTranslationLocale(
        locale,
        qMax(maxWaitTime - timer.elapsed(), kMinInstallationTime));
}

LocaleRollback::~LocaleRollback()
{
    if (m_manager)
        m_manager->setCurrentThreadTranslationLocale(m_prevLocale, 0ms);
}

} // namespace nx::vms::translation
