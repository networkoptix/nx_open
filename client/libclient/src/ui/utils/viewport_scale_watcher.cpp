
#include "viewport_scale_watcher.h"

QnViewportScaleWatcher::QnViewportScaleWatcher(QObject* parent)
    :
    base_type(parent),

    m_viewport(nullptr),
    m_transform(),
    m_scale(1)
{
}

QnViewportScaleWatcher::~QnViewportScaleWatcher()
{
}

void QnViewportScaleWatcher::setScene(QGraphicsScene* scene)
{
    if (m_viewport)
        return;

    m_viewport = (!scene || scene->views().empty() ? nullptr : scene->views().first());
    NX_ASSERT(m_viewport, "Invalid viewport");
    if (!m_viewport)
        return;

    m_viewport->viewport()->installEventFilter(this);
    updateScale();
}

qreal QnViewportScaleWatcher::scale() const
{
    return m_scale;
}

void QnViewportScaleWatcher::updateScale()
{
    const auto transform = m_viewport->viewportTransform();
    if (m_transform == transform)   //< Same transform - same scale
        return;

    const auto newScale = 1.0 / std::sqrt(transform.m11() * transform.m11()
        + transform.m12() * transform.m12());
    if (m_scale == newScale)
        return;

    m_scale = newScale;
    emit scaleChanged();
}

bool QnViewportScaleWatcher::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Paint)
        updateScale();

    return base_type::eventFilter(watched, event);
}
