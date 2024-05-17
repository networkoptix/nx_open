// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringList>
#include <QtCore/QTranslator>

#include <nx/utils/thread/mutex.h>

namespace nx::i18n {

/**
 * TranslationOverlayItem is an overload QTranslator, which processes strings only when called
 * from specified context (currently, only when called from a thread with altered translation).
 *
 * It's used to obtain translation of a string to locale different from the main locale of the app.
 */
class TranslationOverlayItem: public QTranslator
{
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
    mutable nx::ReadWriteLock m_lock;
};

} // namespace nx::i18n
