// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <vector>

#include <QtCore/QString>

#include "preloaded_translation_reference.h"

namespace nx::vms::utils {

class TranslationManager;

/**
 * Stores information about temporary Qt-specific locale used for all translatable strings
 * in the current thread. When this object is destroyed, the original locale is restored.
 */
class NX_VMS_UTILS_API ScopedLocale final
{
    friend TranslationManager;

public:
    ScopedLocale(const PreloadedTranslationReference& translation, std::chrono::milliseconds maxWaitTime);
    ~ScopedLocale();

    ScopedLocale() = default;
    ScopedLocale(const ScopedLocale&) = delete;
    ScopedLocale& operator=(const ScopedLocale&) = delete;
    ScopedLocale(ScopedLocale&&) = default;
    ScopedLocale& operator=(ScopedLocale&&) = default;

    void reset();

    [[nodiscard]]
    static ScopedLocale install(
        TranslationManager* manager, const std::vector<QString>& preferredLocales);

private:
    PreloadedTranslationReference m_translationRef;
    QString m_oldLocale;
    QString m_newLocale;
};

} // namespace nx::vms::utils
