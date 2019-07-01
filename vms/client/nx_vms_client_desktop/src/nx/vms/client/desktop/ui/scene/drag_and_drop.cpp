#include "drag_and_drop.h"

#include <QtCore/QPointer>
#include <QtGui/QDrag>
#include <QtQml/QtQml>
#include <QtQuick/private/qquickpixmapcache_p.h>

#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

namespace {

QPixmap emptyPixmap()
{
    QPixmap result(1, 1);
    result.fill(Qt::transparent);
    return result;
}

QPixmap loadPixmap(const DragAndDrop* context, const QUrl& source)
{
    if (source.isEmpty())
        return emptyPixmap();

    const QQuickPixmap pixmapLoader(qmlEngine(context), source);
    if (!pixmapLoader.image().isNull())
        return QPixmap::fromImage(pixmapLoader.image());

    NX_WARNING(context, "Image \"%1\" cannot be loaded", source);
    return emptyPixmap();
}

} // namespace

Qt::DropAction DragAndDrop::execute(
    QObject* source,
    QMimeData* mimeData,
    Qt::DropActions supportedActions,
    Qt::DropAction proposedAction,
    const QUrl& imageSource,
    const QPointF& hotSpot) const
{
    if (!NX_ASSERT(mimeData))
        return Qt::IgnoreAction;

    if (!NX_ASSERT(source))
    {
        delete mimeData;
        return Qt::IgnoreAction;
    }

    auto drag(new QDrag(source));
    drag->setPixmap(loadPixmap(this, imageSource));
    drag->setHotSpot(hotSpot.toPoint());

    drag->setMimeData(mimeData);
    QQmlEngine::setObjectOwnership(mimeData, QQmlEngine::CppOwnership);

    // The drag object will be automatically deleted afterwards.
    return drag->exec(supportedActions, proposedAction);
}

void DragAndDrop::registerQmlType()
{
    qmlRegisterSingletonType<DragAndDrop>("nx.vms.client.desktop", 1, 0, "DragAndDrop",
        [](QQmlEngine*, QJSEngine*) -> QObject* { return new DragAndDrop(); });

    qRegisterMetaType<QMimeData*>();
}

} // namespace nx::vms::client::desktop
