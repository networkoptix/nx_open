#pragma once

#include "translation.h"

#include <chrono>
#include <memory>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QStringList>

#include <nx/utils/thread/mutex.h>

namespace nx::vms::translation {

class TranslationOverlayItem;

/**
 * Translation overlay is a set of translators with the same locale.
 * It's created by QnTranslationManager with the same set of prefixes as the main translation,
 * so we may be pretty sure that all necessary strings are loaded into the overlay too.
 */
class TranslationOverlay: public QObject
{
public:
    TranslationOverlay(QnTranslation&& translation, QObject* parent = 0);
    virtual ~TranslationOverlay();

    void addRef();
    void removeRef();

    bool isInstalled() const;
    void waitForInstallation(std::chrono::milliseconds maxWaitTime);

    void addThreadContext(const Qt::HANDLE& context);
    void removeThreadContext(const Qt::HANDLE& context);

private:
    void handleTranslatorsUnderMutex();

private:
    QnTranslation m_translation;
    std::vector<std::unique_ptr<TranslationOverlayItem>> m_translators;

    nx::Mutex m_mutex;
    int m_refCount = 0;
    QList<Qt::HANDLE> m_threads;
    nx::WaitCondition m_installedCondition;
    std::atomic_bool m_installed = false;
};

} // namespace nx::vms::translation
