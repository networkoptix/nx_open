// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "translation_overlay.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>

#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log_main.h>

#include "translation_overlay_item.h"

namespace nx::i18n {

TranslationOverlay::TranslationOverlay(Translation&& translation, QObject* parent):
    QObject(parent),
    m_translation(translation)
{
    for (const QString& file: translation.filePaths)
    {
        auto translator = std::make_unique<TranslationOverlayItem>();
        if (translator->load(file))
        {
            // Translator can be parented by qApp in some cases. Push it to the right thread.
            translator->moveToThread(qApp->thread());
            m_translators.push_back(std::move(translator));
        }
    }
}

TranslationOverlay::~TranslationOverlay()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_installed)
    {
        // The only sane scenario: app is shutting down.
        NX_DEBUG(
            this,
            "Translation overlay for locale %1 is still being used on destruction (ref count: %2)",
            m_translation.localeCode,
            m_refCount);

        // Translators can't be deleted when they are in use. Therefore we need to release them
        // and attach to qApp to avoid memory leak. Since setParent sends signals internally,
        // we can call it only from the application thread.
        QList<TranslationOverlayItem*> translators;
        for (auto& t: m_translators)
        {
            translators << t.release();
        }
        const auto callback =
            [translators]()
            {
                for (auto& translator: translators)
                {
                    translator->setParent(qApp);
                }
            };
        QMetaObject::invokeMethod(qApp, callback, Qt::QueuedConnection);
    }
}

void TranslationOverlay::addRef()
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_refCount++;
    if (m_refCount == 1)
        handleTranslatorsUnderMutex();
}

void TranslationOverlay::removeRef()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (--m_refCount == 0)
        handleTranslatorsUnderMutex();
}

bool TranslationOverlay::isInstalled() const
{
    return m_installed;
}

void TranslationOverlay::waitForInstallation(std::chrono::milliseconds maxWaitTime)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!m_installed)
        m_installedCondition.wait(&m_mutex, maxWaitTime);
}

void TranslationOverlay::addThreadContext(const Qt::HANDLE& context)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    for (auto& translator: m_translators)
    {
        translator->addThreadContext(context);
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

void TranslationOverlay::handleTranslatorsUnderMutex()
{
    // QCoreApplication sends a signal for itself inside of qApp->installTranslator().
    // As result, it is impossible to install translators from any other thread.
    if (QThread::currentThread() == qApp->thread())
    {
        if (m_refCount > 0 && !m_installed)
        {
            for (auto& translator: m_translators)
                qApp->installTranslator(translator.get());

            m_installed = true;
            m_installedCondition.wakeAll();
        }
        if (m_installed && m_refCount == 0)
        {
            for (auto& translator: m_translators)
                qApp->removeTranslator(translator.get());

            m_installed = false;
        }
    }
    else
    {
        // Call the same method from the main thread.
        const auto callback = nx::utils::guarded(this,
            [this]()
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                handleTranslatorsUnderMutex();
            });
        QMetaObject::invokeMethod(qApp, callback, Qt::QueuedConnection);
    }
}

} // namespace nx::i18n
