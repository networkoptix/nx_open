#pragma once

#include "translation.h"

#include <memory>
#include <vector>

#include <QStringList>

#include <nx/utils/thread/mutex.h>

namespace nx::vms::translation {

class TranslationOverlayItem;

/**
 * Translation overlay is a set of translators with the same locale.
 * It's created by QnTranslationManager with the same set of prefixes as the main translation,
 * so we may be pretty sure that all necessary strings are loaded into the overlay too.
 */
class TranslationOverlay
{
public:
    TranslationOverlay(QnTranslation&& translation);
    virtual ~TranslationOverlay();

    void addThreadContext(const Qt::HANDLE& context);
    void removeThreadContext(const Qt::HANDLE& context);

    void uninstallIfUnused();

private:
    QnTranslation m_translation;
    std::vector<std::unique_ptr<TranslationOverlayItem>> m_translators;

    QList<Qt::HANDLE> m_threads;
    nx::utils::Mutex m_mutex;
};

} // namespace nx::vms::translation
