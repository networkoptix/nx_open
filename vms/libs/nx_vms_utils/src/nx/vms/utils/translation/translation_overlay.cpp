// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "translation_overlay.h"

#include "translation_overlay_item.h"

#include <QCoreApplication>
#include <QtCore/QThread>

#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log_main.h>

namespace nx::vms::utils {

TranslationOverlay::TranslationOverlay(Translation&& translation, QObject* parent):
    QObject(parent),
    m_translation(translation)
{
    for (const QString& file: translation.filePaths)
    {
        auto translator = std::make_unique<TranslationOverlayItem>();
        translator->moveToThread(qApp->thread());

        if (translator->load(file))
            m_translators.push_back(std::move(translator));
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

        for (auto& translator: m_translators)
        {
            // Release translators to avoid possible crashes. Translator is parented by qApp in
            // handleTranslatorsUnderMutex(), therefore it will be deleted later.
            translator.release();
        }
        m_translators.clear();
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
        for (auto& translator: m_translators)
        {
            // Set parent to avoid memory leaks in the case of out-of-order destruction. Parent
            // should be set in qApp's thread because some signals are sent internally.
            if (!translator->parent())
                translator->setParent(qApp);
        }

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

} // namespace nx::vms::utils
