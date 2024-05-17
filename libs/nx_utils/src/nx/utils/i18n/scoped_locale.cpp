// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "scoped_locale.h"

#include <nx/utils/log/log.h>

#include "translation_manager.h"

namespace {

const QString kDefaultAppLanguage = "default app language";

} // namespace

namespace nx::i18n {

ScopedLocale::ScopedLocale(
    const PreloadedTranslationReference& translation,
    milliseconds maxWaitTime)
    :
    m_translationRef(translation),
    m_newLocale(translation.locale())
{
    if (const auto& manager = m_translationRef.manager())
    {
        m_oldLocale = TranslationManager::getCurrentThreadLocale();
        if (!manager->setCurrentThreadTranslationLocale(m_newLocale, maxWaitTime))
            m_newLocale.clear(); //< We use it for safety check in destructor.

        const auto& result = m_newLocale.isEmpty() ? kDefaultAppLanguage : m_newLocale;
        NX_VERBOSE(this, "Switched locale %1 to %2", m_oldLocale, result);
    }
}

ScopedLocale::~ScopedLocale()
{
    if (const auto& manager = m_translationRef.manager())
    {
        const auto curLocale = TranslationManager::getCurrentThreadLocale();
        NX_ASSERT(
            curLocale == m_newLocale,
            "Locale scopes aren't nested: current locale (%1) does not match expected value (%2)",
            curLocale,
            m_newLocale);

        manager->setCurrentThreadTranslationLocale(m_oldLocale, 0ms);
        NX_VERBOSE(this, "Restored locale %1", m_oldLocale);
    }
}

} // namespace nx::i18n
