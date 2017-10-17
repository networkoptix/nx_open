#include "nx_globals_object.h"

#include <QtCore/QtMath>
#include <QtQuick/private/qquickflickable_p.h>

namespace detail {

QQuickFlickable* findFlickable(const QQuickItem* item)
{
    if (!item)
        return nullptr;

    auto parent = item->parentItem();
    while (parent)
    {
        if (auto flickable = qobject_cast<QQuickFlickable*>(parent))
            return flickable;

        parent = parent->parentItem();
    }

    return nullptr;
}

bool itemIsAncestorOf(QQuickItem* item, QQuickItem* parent)
{
    if (!item || !parent)
        return false;

    while ((item = item->parentItem()))
    {
        if (item == parent)
            return true;
    }

    return false;
}

} using namespace detail;

NxGlobalsObject::NxGlobalsObject(QObject* parent):
    QObject(parent)
{
}

QnUrlHelper NxGlobalsObject::url(const nx::utils::Url& url) const
{
    return QnUrlHelper(url);
}

QnSoftwareVersion NxGlobalsObject::softwareVersion(const QString& version) const
{
    return QnSoftwareVersion(version);
}

void NxGlobalsObject::ensureFlickableChildVisible(QQuickItem* item)
{
    if (!item)
        return;

    auto flickable = findFlickable(item);
    if (!flickable)
        return;

    const auto contentItem = flickable->contentItem();
    if (!contentItem || !itemIsAncestorOf(item, contentItem))
        return;

    const auto rect = item->mapRectToItem(contentItem,
        QRect(0, 0, static_cast<int>(item->width()), static_cast<int>(item->height())));

    auto adjustContentPosition =
        [](qreal position, qreal contentSize, qreal flickableSize,
            qreal startMargin, qreal endMargin,
            qreal itemPosition, qreal itemSize)
        {
            const auto itemEnd = itemPosition + itemSize - position;
            if (itemEnd > flickableSize)
                position += (itemEnd - flickableSize);

            const auto itemStart = itemPosition - position;
            if (itemStart < 0)
                position += itemStart;

            position = qBound(-startMargin, position, contentSize - flickableSize + endMargin);

            return position;
        };

    flickable->setContentX(adjustContentPosition(
        flickable->contentX(), flickable->contentWidth(), flickable->width(),
        flickable->leftMargin(), flickable->rightMargin(),
        rect.x(), rect.width()));
    flickable->setContentY(adjustContentPosition(
        flickable->contentY(), flickable->contentHeight(), flickable->height(),
        flickable->topMargin(), flickable->bottomMargin(),
        rect.y(), rect.height()));
}
