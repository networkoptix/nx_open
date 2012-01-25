#include "viewport_geometry_accessor.h"
#include <QGraphicsView>
#include <QVariant>
#include <utils/common/checked_cast.h>
#include <ui/common/scene_utility.h>

ViewportGeometryAccessor::ViewportGeometryAccessor():
    m_marginFlags(Qn::MARGINS_AFFECT_POSITION | Qn::MARGINS_AFFECT_SIZE)
{}

    // XXX mapRectToScene(m_view, (m_marginFlags & MARGINS_AFFECT_SIZE) ? eroded(m_view->viewport()->rect(), m_viewportMargins) : m_view->viewport()->rect());

QVariant ViewportGeometryAccessor::get(const QObject *object) const {
    const QGraphicsView *view = checked_cast<const QGraphicsView *>(object);

    QRect viewportRect = view->viewport()->rect();
    if(m_marginFlags & Qn::MARGINS_AFFECT_SIZE)
        viewportRect = SceneUtility::eroded(viewportRect, m_margins);

    return SceneUtility::mapRectToScene(view, viewportRect);
}

void ViewportGeometryAccessor::set(QObject *object, const QVariant &value) const {
    QGraphicsView *view = checked_cast<QGraphicsView *>(object);
    QRectF sceneRect = value.toRectF();

    /* Extend to match aspect ratio. */
    sceneRect = aspectRatioAdjusted(view, sceneRect);

    /* Adjust for margins. */
    MarginsF extension = SceneUtility::cwiseDiv(m_margins, QSizeF(view->viewport()->size()));
    extension = SceneUtility::cwiseMul(SceneUtility::cwiseDiv(extension, MarginsF(1.0, 1.0, 1.0, 1.0) - extension), sceneRect.size());
    sceneRect = SceneUtility::dilated(sceneRect, extension);

    /* Set! */
    SceneUtility::moveViewportSceneTo(view, sceneRect.center());
    SceneUtility::scaleViewportTo(view, sceneRect.size(), Qt::KeepAspectRatioByExpanding);
}

QRectF ViewportGeometryAccessor::aspectRatioAdjusted(QGraphicsView *view, const QRectF &sceneRect) const {
    QSize viewportSize;
    
    if(m_marginFlags & Qn::MARGINS_AFFECT_SIZE) {
        viewportSize = SceneUtility::eroded(view->viewport()->size(), m_margins);
    } else {
        viewportSize = view->viewport()->size();
    }

    return SceneUtility::expanded(
        SceneUtility::aspectRatio(viewportSize), 
        sceneRect, 
        Qt::KeepAspectRatioByExpanding, 
        Qt::AlignCenter
    );
}


