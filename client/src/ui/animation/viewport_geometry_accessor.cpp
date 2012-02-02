#include "viewport_geometry_accessor.h"
#include <QGraphicsView>
#include <QVariant>
#include <utils/common/checked_cast.h>
#include <ui/common/scene_utility.h>

ViewportGeometryAccessor::ViewportGeometryAccessor():
    m_marginFlags(Qn::MARGINS_AFFECT_POSITION | Qn::MARGINS_AFFECT_SIZE)
{}

QVariant ViewportGeometryAccessor::get(const QObject *object) const {
    const QGraphicsView *view = checked_cast<const QGraphicsView *>(object);

    QSize size = view->viewport()->size();
    if(m_marginFlags & Qn::MARGINS_AFFECT_SIZE)
        size = SceneUtility::eroded(size, m_margins);

    QPoint center = view->viewport()->rect().center(); // TODO: check whether rounding causes issues.
    if(m_marginFlags & Qn::MARGINS_AFFECT_POSITION)
        center = SceneUtility::eroded(view->viewport()->rect(), m_margins).center();

    return SceneUtility::mapRectToScene(view, QRect(center - SceneUtility::toPoint(size) / 2, size));
}

void ViewportGeometryAccessor::set(QObject *object, const QVariant &value) const {
    QGraphicsView *view = checked_cast<QGraphicsView *>(object);
    QRectF sceneRect = value.toRectF();

    /* Adjust to match margins. */
    sceneRect = adjustedToViewport(view, sceneRect);

    /* Expand if margins apply to size. */
    if(m_marginFlags & Qn::MARGINS_AFFECT_SIZE) {
        MarginsF extension = SceneUtility::cwiseDiv(m_margins, QSizeF(view->viewport()->size()));
                 extension = SceneUtility::cwiseDiv(extension, MarginsF(1.0, 1.0, 1.0, 1.0) - extension);
                 extension = SceneUtility::cwiseMul(extension, sceneRect.size());
        sceneRect = SceneUtility::dilated(sceneRect, extension);
    }

    /* Set! */
    SceneUtility::moveViewportSceneTo(view, sceneRect.center());
    SceneUtility::scaleViewportTo(view, sceneRect.size(), Qt::KeepAspectRatioByExpanding);
}

QRectF ViewportGeometryAccessor::adjustedToViewport(QGraphicsView *view, const QRectF &sceneRect) const {
    /* Calculate resulting size. */
    QSizeF size;
    {
        QSize viewportSize;
        if(m_marginFlags & Qn::MARGINS_AFFECT_SIZE) {
            viewportSize = SceneUtility::eroded(view->viewport()->size(), m_margins);
        } else {
            viewportSize = view->viewport()->size();
        }
        size = SceneUtility::expanded(SceneUtility::aspectRatio(viewportSize), sceneRect.size(), Qt::KeepAspectRatioByExpanding);
    }

    /* Calculate resulting position. */
    QPointF center;
    {
        QRectF erodedViewportSceneRect;
        QRectF viewportSceneRect;
        if(m_marginFlags & Qn::MARGINS_AFFECT_SIZE) {
            erodedViewportSceneRect = QRectF(sceneRect.center() - SceneUtility::toPoint(size), size);
            MarginsF extension = SceneUtility::cwiseDiv(m_margins, view->viewport()->size());
                     extension = SceneUtility::cwiseDiv(extension, MarginsF(1.0, 1.0, 1.0, 1.0) - extension);
                     extension = SceneUtility::cwiseMul(extension, erodedViewportSceneRect.size());
            viewportSceneRect = SceneUtility::dilated(erodedViewportSceneRect, extension);
        } else {
            viewportSceneRect = QRectF(sceneRect.center() - SceneUtility::toPoint(size) / 2, size);
            erodedViewportSceneRect = SceneUtility::eroded(viewportSceneRect, SceneUtility::cwiseMul(viewportSceneRect.size(), SceneUtility::cwiseDiv(m_margins, view->viewport()->size())));
        }
        
        if(!(m_marginFlags & Qn::MARGINS_AFFECT_POSITION)) {
            center = viewportSceneRect.center();
        } else {
            center = erodedViewportSceneRect.center();
            center.rx() = qBound(viewportSceneRect.left() + sceneRect.width()  / 2, center.x(), viewportSceneRect.right()  - sceneRect.width()  / 2);
            center.ry() = qBound(viewportSceneRect.top()  + sceneRect.height() / 2, center.y(), viewportSceneRect.bottom() - sceneRect.height() / 2);

            center = viewportSceneRect.center() + (viewportSceneRect.center() - center);
        }
    }

    return QRectF(
        center - SceneUtility::toPoint(size) / 2,
        size
    );
}


