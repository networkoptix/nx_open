// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtCore/QString>

namespace nx::i18n {

class TranslationManager;

class NX_UTILS_API PreloadedTranslationReference
{
    friend TranslationManager;

public:
    PreloadedTranslationReference(); //< Creates an invalid (empty) reference.
    ~PreloadedTranslationReference();

    PreloadedTranslationReference(const PreloadedTranslationReference& other);
    PreloadedTranslationReference& operator=(const PreloadedTranslationReference& other);

    QString locale() const;
    QPointer<TranslationManager> manager() const;

private:
    PreloadedTranslationReference(TranslationManager* manager, const QString& locale);

private:
    QPointer<TranslationManager> m_manager;
    QString m_locale;
};

} // namespace nx::i18n
