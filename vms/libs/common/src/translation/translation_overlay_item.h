#pragma once

#include <QtCore/QTranslator>
#include <QtCore/QReadWriteLock>
#include <QtCore/QStringList>

namespace nx::vms::translation {

/**
 * TranslationOverlayItem is an overload QTranslator, which processes strings only when called
 * from specified context (currently, only when called from a thread with altered translation).
 *
 * It's used to obtain translation of a string to locale different from the main locale of the app.
 */
class TranslationOverlayItem: public QTranslator
{
    Q_OBJECT

public:
    TranslationOverlayItem(QObject* parent = nullptr);
    virtual ~TranslationOverlayItem();

    void addThreadContext(const Qt::HANDLE& context);
    void removeThreadContext(const Qt::HANDLE& context);

    virtual QString translate(
        const char* context,
        const char* sourceText,
        const char* disambiguation = nullptr,
        int n = -1) const override;

private:
    QList<Qt::HANDLE> m_threads;
    mutable QReadWriteLock m_lock;
};

} //namespace nx::vms::translation
