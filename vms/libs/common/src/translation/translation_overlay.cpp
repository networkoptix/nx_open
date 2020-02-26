#include "translation_overlay.h"

#include "translation_overlay_item.h"

#include <QCoreApplication>
#include <QtCore/QThread>

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

    if (!m_installed)
    {
        const auto installTranslators =
            [this]()
            {
                for (auto& translator: m_translators)
                    qApp->installTranslator(translator.get());
            };

        // QCoreApplication sends a signal for itself inside of qApp->installTranslator().
        // As result, it is impossible to call this method from any other thread.
        if (QThread::currentThread() == qApp->thread())
            installTranslators();
        else
            QMetaObject::invokeMethod(qApp, installTranslators, Qt::BlockingQueuedConnection);

        m_installed = true;
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

    if (m_installed)
    {
        auto uninstallTranslators =
            [this]()
            {
                for (auto& translator: m_translators)
                    qApp->removeTranslator(translator.get());
            };

        // QCoreApplication sends a signal for itself inside of qApp->removeTranslator().
        // As result, it is impossible to call this method from any other thread.
        if (QThread::currentThread() == qApp->thread())
            uninstallTranslators();
        else
            QMetaObject::invokeMethod(qApp, uninstallTranslators, Qt::BlockingQueuedConnection);

        m_installed = false;
    }
}

} // namespace nx::vms::translation
