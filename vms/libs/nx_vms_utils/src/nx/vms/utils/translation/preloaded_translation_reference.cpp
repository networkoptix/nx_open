// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "preloaded_translation_reference.h"

#include <nx/vms/utils/translation/translation_manager.h>

namespace nx::vms::utils {

PreloadedTranslationReference::PreloadedTranslationReference()
{
}

PreloadedTranslationReference::PreloadedTranslationReference(
    TranslationManager* manager,
    const QString& locale)
    :
    m_manager(manager),
    m_locale(locale)
{
    if (m_manager)
        m_manager->addPreloadedTranslationReference(locale);
}

PreloadedTranslationReference::~PreloadedTranslationReference()
{
    reset();
}

PreloadedTranslationReference::PreloadedTranslationReference(
    const PreloadedTranslationReference& other)
    :
    m_manager(other.m_manager),
    m_locale(other.m_locale)
{
    if (m_manager)
        m_manager->addPreloadedTranslationReference(m_locale);
}

PreloadedTranslationReference& PreloadedTranslationReference::operator=(
    const PreloadedTranslationReference& other)
{
    if (m_manager)
        m_manager->removePreloadedTranslationReference(m_locale);

    m_manager = other.m_manager;
    m_locale = other.m_locale;

    if (m_manager)
        m_manager->addPreloadedTranslationReference(m_locale);

    return *this;
}

QString PreloadedTranslationReference::locale() const
{
    return m_locale;
}

QPointer<TranslationManager> PreloadedTranslationReference::manager() const
{
    return m_manager;
}

void PreloadedTranslationReference::reset()
{
    if (m_manager)
        m_manager->removePreloadedTranslationReference(m_locale);
    m_manager.clear();
    m_locale.clear();
}

} // namespace nx::vms::utils
