#include "translation_overlay.h"

#include "translation_overlay_item.h"

#include <QCoreApplication>

namespace nx::vms::translation {

TranslationOverlay::TranslationOverlay(QnTranslation&& translation):
    m_translation(translation)
{
    for (const QString& file: translation.filePaths())
    {
        auto translator = std::make_unique<TranslationOverlayItem>();
        if (translator->load(file))
            m_translators.push_back(std::move(translator));
    }
}

TranslationOverlay::~TranslationOverlay()
{
}

void TranslationOverlay::addThreadContext(const Qt::HANDLE& context)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    for (auto& translator: m_translators)
    {
        translator->addThreadContext(context);
    }

    if (m_threads.empty())
    {
        for (auto& translator: m_translators)
            qApp->installTranslator(translator.get());
    }

    m_threads << context;
}

void TranslationOverlay::removeThreadContext(const Qt::HANDLE& context)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    for (auto& translator: m_translators)
    {
        translator->removeThreadContext(context);
    }

    m_threads.removeAll(context);
}

void TranslationOverlay::uninstallIfUnused()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_threads.empty())
    {
        for (auto& translator: m_translators)
            qApp->removeTranslator(translator.get());
    }
}

} // namespace nx::vms::translation
