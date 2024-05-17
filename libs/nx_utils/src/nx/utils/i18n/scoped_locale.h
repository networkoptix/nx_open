// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>

#include <QtCore/QPointer>
#include <QtCore/QString>

#include "preloaded_translation_reference.h"

namespace nx::i18n {

using namespace std::chrono;

class TranslationManager;

/**
 * Stores information about temporary Qt-specific locale used for all translatable strings
 * in the current thread. When this object is destroyed, the original locale is restored.
 */
class NX_UTILS_API ScopedLocale
{
    friend TranslationManager;

public:
    ScopedLocale(const PreloadedTranslationReference& translation, milliseconds maxWaitTime);
    ~ScopedLocale();

private:
    PreloadedTranslationReference m_translationRef;
    QString m_oldLocale;
    QString m_newLocale;
};

using ScopedLocalePtr = std::unique_ptr<ScopedLocale>;

} // namespace nx::i18n
