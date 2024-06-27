// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QStringList>

#include <nx/utils/thread/mutex.h>

#include "translation.h"

namespace nx::i18n {

class TranslationOverlayItem;

/**
 * Translation overlay is a set of translators with the same locale. It's created by
 * TranslationManager with the same set of prefixes as the main translation, so we may be pretty
 * sure that all necessary strings are loaded into the overlay too.
 */
class TranslationOverlay: public QObject
{
public:
    TranslationOverlay(Translation&& translation, QObject* parent = 0);
    virtual ~TranslationOverlay();

    void addRef();
    void removeRef();

    bool isInstalled() const;

    /** Check whether there are references on this overlay. */
    bool isUsed() const;

    void waitForInstallation(std::chrono::milliseconds maxWaitTime);

    void addThreadContext(const Qt::HANDLE& context);
    void removeThreadContext(const Qt::HANDLE& context);

private:
    void handleTranslatorsUnderMutex();

private:
    Translation m_translation;
    std::vector<std::unique_ptr<TranslationOverlayItem>> m_translators;

    mutable nx::Mutex m_mutex;
    int m_refCount = 0;
    QList<Qt::HANDLE> m_threads;
    nx::WaitCondition m_installedCondition;
    std::atomic_bool m_installed = false;
};

} // namespace nx::i18n
