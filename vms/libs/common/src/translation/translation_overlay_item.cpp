#include "translation_overlay_item.h"

#include <QtCore/QThread>

namespace nx::vms::translation {

TranslationOverlayItem::TranslationOverlayItem(QObject* parent):
    QTranslator(parent)
{
}

TranslationOverlayItem::~TranslationOverlayItem()
{
}

void TranslationOverlayItem::addThreadContext(const Qt::HANDLE& context)
{
    QWriteLocker l(&m_lock);
    m_threads << context;
}

void TranslationOverlayItem::removeThreadContext(const Qt::HANDLE& context)
{
    QWriteLocker l(&m_lock);
    m_threads.removeAll(context);
}

QString TranslationOverlayItem::translate(
        const char* context,
        const char* sourceText,
        const char* disambiguation,
        int n) const
{
    QReadLocker l(&m_lock);

    return m_threads.contains(QThread::currentThreadId())
        ? QTranslator::translate(context, sourceText, disambiguation, n)
        : QString();
}

} //namespace nx::vms::translation
