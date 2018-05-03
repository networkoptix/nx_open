#include "cursor_manager.h"

#include <QtQuick/QQuickItem>

#include <ui/common/cursor_cache.h>

namespace nx {
namespace client {
namespace desktop {

class CursorManager::Private
{
public:
    QObject* target = nullptr;
    QQuickItem* targetQuickItem = nullptr;
    QObject* requester = nullptr;

    QScopedPointer<QnCursorCache> cursorCache;
};

CursorManager::CursorManager(QObject* parent):
    QObject(parent),
    d(new Private())
{
    d->cursorCache.reset(new QnCursorCache());
}

CursorManager::~CursorManager()
{
}

QObject* CursorManager::target() const
{
    return d->target;
}

void CursorManager::setTarget(QObject* target)
{
    if (d->target == target)
        return;

    if (d->target)
        unsetCursor();

    d->targetQuickItem = qobject_cast<QQuickItem*>(target);
    d->target = d->targetQuickItem;
    d->requester = nullptr;

    emit targetChanged();
}

void CursorManager::setCursor(QObject* requester, Qt::CursorShape shape, int rotation)
{
    if (!d->targetQuickItem)
        return;

    d->requester = requester;

    const auto& cursor = d->cursorCache->cursor(shape, rotation);

    if (d->targetQuickItem)
        d->targetQuickItem->setCursor(cursor);
}

void CursorManager::setCursorForFrameSection(QObject* requester, int section, int rotation)
{
    if (!d->targetQuickItem)
        return;

    d->requester = requester;

    if (section == core::FrameSection::NoSection)
    {
        unsetCursor(requester);
        return;
    }

    const auto& cursor = d->cursorCache->cursorForWindowSection(
        core::FrameSection::toQtWindowFrameSection(
            static_cast<core::FrameSection::Section>(section)),
        rotation);

    if (d->targetQuickItem)
        d->targetQuickItem->setCursor(cursor);
}

void CursorManager::setCursorForFrameSection(int section, int rotation)
{
    setCursorForFrameSection(nullptr, section, rotation);
}

void CursorManager::unsetCursor(QObject* requester)
{
    if (requester && d->requester != requester)
        return;

    d->requester = nullptr;

    if (d->targetQuickItem)
        d->targetQuickItem->unsetCursor();
}

void CursorManager::registerQmlType()
{
    qmlRegisterType<CursorManager>("nx.client.desktop", 1, 0, "CursorManager");
}

} // namespace desktop
} // namespace client
} // namespace nx
