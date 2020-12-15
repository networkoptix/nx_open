#include "preloaded_translation_reference.h"

#include <translation/translation_manager.h>

namespace nx::vms::translation {

PreloadedTranslationReference::PreloadedTranslationReference()
{
}

PreloadedTranslationReference::PreloadedTranslationReference(
    QnTranslationManager* manager,
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
    if (m_manager)
        m_manager->removePreloadedTranslationReference(m_locale);
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

QPointer<QnTranslationManager> PreloadedTranslationReference::manager() const
{
    return m_manager;
}

} // namespace nx::vms::translation
