#include "drag_and_drop_p.h"

#include <QtGui/QDropEvent>

#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

bool DragMimeDataWatcher::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_watched)
        return false;

    const auto clearMimeData =
        [this]() { setMimeData({}); };

    switch (event->type())
    {
        case QEvent::Drop:
            executeLater(clearMimeData, this);
            [[fallthrough]];
        case QEvent::DragEnter:
        case QEvent::DragMove:
            setMimeData(const_cast<QMimeData*>(static_cast<QDropEvent*>(event)->mimeData()));
            break;

        case QEvent::DragLeave:
            clearMimeData();
            break;

        default:
            break;
    }

    return false;
}

QObject* DragMimeDataWatcher::watched() const
{
    return m_watched;
}

void DragMimeDataWatcher::setWatched(QObject* value)
{
    if (value == m_watched)
        return;

    if (m_watched)
    {
        m_watched->disconnect(this);
        m_watched->removeEventFilter(this);
    }

    const auto emitWatchedChanged =
        [this]() { emit watchedChanged({}); };

    m_watched = value;
    if (m_watched)
    {
        m_watched->installEventFilter(this);
        connect(m_watched.data(), &QObject::destroyed, this, emitWatchedChanged);
    }

    emitWatchedChanged();
}

QMimeData* DragMimeDataWatcher::mimeData() const
{
    return m_mimeData;
}

void DragMimeDataWatcher::setMimeData(QMimeData* value)
{
    if (value == m_mimeData)
        return;

    if (m_mimeData)
        m_mimeData->disconnect(this);

    const auto emitMimeDataChanged =
        [this]() { emit mimeDataChanged({}); };

    m_mimeData = value;
    if (m_mimeData)
        connect(m_mimeData.data(), &QObject::destroyed, this, emitMimeDataChanged);

    emitMimeDataChanged();
}

} // namespace nx::vms::client::desktop
