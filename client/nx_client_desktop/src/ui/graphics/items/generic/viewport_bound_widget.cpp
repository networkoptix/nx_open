#include "viewport_bound_widget.h"

#include <QtWidgets/QGraphicsScale>

#include <nx/utils/math/fuzzy.h>

#include <ui/graphics/instruments/transform_listener_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/utils/viewport_scale_watcher.h>

QnViewportBoundWidget::QnViewportBoundWidget(QGraphicsItem* parent):
    base_type(parent),
    m_fixedSize(QSizeF(0.0, 0.0)),
    m_scale(new QGraphicsScale(this)),
    m_scaleWatcher(new QnViewportScaleWatcher(this))
{
    QList<QGraphicsTransform*> transformations = this->transformations();
    transformations.push_back(m_scale);
    setTransformations(transformations);
    setAcceptedMouseButtons(Qt::NoButton);

    itemChange(ItemSceneHasChanged, QVariant::fromValue(scene()));

    connect(m_scaleWatcher, &QnViewportScaleWatcher::scaleChanged, this,
        &QnViewportBoundWidget::updateScale);

    connect(m_scale, &QGraphicsScale::xScaleChanged, this,
        &QnViewportBoundWidget::scaleChanged);
}

QnViewportBoundWidget::~QnViewportBoundWidget()
{
}

const QSizeF& QnViewportBoundWidget::fixedSize()
{
    return m_fixedSize;
}

void QnViewportBoundWidget::setFixedSize(const QSizeF& fixedSize)
{
    if (qFuzzyEquals(m_fixedSize, fixedSize))
        return;

    m_fixedSize = fixedSize;
    updateScale();
}

void QnViewportBoundWidget::updateScale()
{
    if (m_fixedSize.isNull())
        return;

    qreal scale = m_scaleWatcher->scale();

    QRectF geometry = QRectF(pos(), m_fixedSize / scale);
    base_type::setGeometry(geometry);

    QSizeF resultingSize = size();
    if (resultingSize.width() > geometry.width() || resultingSize.height() > geometry.height())
    {
        const qreal k = (qFuzzyIsNull(geometry.width()) || qFuzzyIsNull(geometry.height()))
            ? 1.0e6
            : qMax(resultingSize.width() / geometry.width(), resultingSize.height() / geometry.height());

        geometry.setSize(geometry.size() * k);
        scale /= k;
        base_type::setGeometry(geometry);
    }

    /* Assume affine transform that does not change x/y scale separately. */
    m_scale->setYScale(scale);
    m_scale->setXScale(scale);
}

qreal QnViewportBoundWidget::scale() const
{
    return m_scale->xScale();
}

qreal QnViewportBoundWidget::sceneScale() const
{
    return m_scaleWatcher->scale();
}

qreal QnViewportBoundWidget::relativeScale() const
{
    return scale() / sceneScale();
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
QVariant QnViewportBoundWidget::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemSceneHasChanged)
    {
        if (scene())
            m_scaleWatcher->initialize(scene());
    }

    return base_type::itemChange(change, value);
}

void QnViewportBoundWidget::setGeometry(const QRectF& geometry)
{
    QSizeF oldSize = size();
    base_type::setGeometry(geometry);
    if (!qFuzzyEquals(size(), oldSize))
        updateScale();
}
