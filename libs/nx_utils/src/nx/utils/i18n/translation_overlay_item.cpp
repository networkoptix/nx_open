// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "translation_overlay_item.h"

#include <QtCore/QThread>

namespace nx::i18n {

TranslationOverlayItem::TranslationOverlayItem(QObject* parent):
    QTranslator(parent)
{
}

TranslationOverlayItem::~TranslationOverlayItem()
{
}

void TranslationOverlayItem::addThreadContext(const Qt::HANDLE& context)
{
    NX_WRITE_LOCKER l(&m_lock);
    m_threads << context;
}

void TranslationOverlayItem::removeThreadContext(const Qt::HANDLE& context)
{
    NX_WRITE_LOCKER l(&m_lock);
    m_threads.removeAll(context);
}

QString TranslationOverlayItem::translate(
        const char* context,
        const char* sourceText,
        const char* disambiguation,
        int n) const
{
    NX_READ_LOCKER l(&m_lock);

    return m_threads.contains(QThread::currentThreadId())
        ? QTranslator::translate(context, sourceText, disambiguation, n)
        : QString();
}

} // namespace nx::i18n
