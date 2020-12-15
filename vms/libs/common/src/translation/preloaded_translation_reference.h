#pragma once

#include <QtCore/QPointer>
#include <QtCore/QString>

class QnTranslationManager;

namespace nx::vms::translation {

class PreloadedTranslationReference
{
    friend QnTranslationManager;

public:
    PreloadedTranslationReference(); //< Creates an invalid (empty) reference.
    ~PreloadedTranslationReference();

    PreloadedTranslationReference(const PreloadedTranslationReference& other);
    PreloadedTranslationReference& operator=(const PreloadedTranslationReference& other);

    QString locale() const;
    QPointer<QnTranslationManager> manager() const;

private:
    PreloadedTranslationReference(QnTranslationManager* manager, const QString& locale);

private:
    QPointer<QnTranslationManager> m_manager;
    QString m_locale;
};

} // namespace nx::vms::translation
