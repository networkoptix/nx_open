#include "viewport_bound_widget.h"
#include <cmath> /* For std::sqrt. */
#include <QGraphicsView>
#include <utils/common/fuzzy.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/warnings.h>
#include <ui/graphics/instruments/transform_listener_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>


QnViewportBoundWidget::QnViewportBoundWidget(QGraphicsItem *parent):
    base_type(parent),
    m_desiredSize(QSizeF(0.0, 0.0)),
    m_inUpdateScale(false)
{
    itemChange(ItemSceneHasChanged, QVariant::fromValue(scene()));
}

QnViewportBoundWidget::~QnViewportBoundWidget() {
    return;
}

const QSizeF &QnViewportBoundWidget::desiredSize() {
    return m_desiredSize;
}

void QnViewportBoundWidget::setDesiredSize(const QSizeF &desiredSize) {
    if(qFuzzyCompare(m_desiredSize, desiredSize))
        return;

    m_desiredSize = desiredSize;

    updateScale();
}

void QnViewportBoundWidget::setDesiredRotation(Qn::FixedItemRotation fixedRotation){
    if (m_fixedRotation == fixedRotation)
        return;

    m_fixedRotation = fixedRotation;
    updateScale();
}

void QnViewportBoundWidget::updateScale(QGraphicsView *view) {
    if (m_desiredSize.isNull())
        return;

    if(!view)
        view = m_lastView.data();
    if(!view && scene() && !scene()->views().empty())
        view = scene()->views()[0];
    if(!view)
        return;
    m_lastView = view;

    if(!isVisible())
        return;

    QnScopedValueRollback<bool> guard(&m_inUpdateScale, true);

    /* Assume affine transform that does not change x/y scale separately. */
    QTransform sceneToViewport = view->viewportTransform();
    qreal scale = 1.0 / std::sqrt(sceneToViewport.m11() * sceneToViewport.m11() + sceneToViewport.m12() * sceneToViewport.m12());

    QSizeF geometrySize = m_desiredSize / scale;
    if (m_fixedRotation == Qn::Angle90 || m_fixedRotation == Qn::Angle270)
        geometrySize.transpose();

    QRectF geometry = QRectF(QPointF(0, 0), geometrySize);
    setGeometry(geometry);

    QSizeF resultingSize = size();
    if(resultingSize.width() > geometry.width() || resultingSize.height() > geometry.height()) {
        qreal k = qMax(resultingSize.width() / geometry.width(), resultingSize.height() / geometry.height());

        geometry.setSize(geometry.size() * k);
        scale /= k;

        setGeometry(geometry);
    }

    QPointF rotationCenter = geometry.center();
    qreal rotation;
    switch (m_fixedRotation){
        case Qn::Angle90:
            rotationCenter.setX(rotationCenter.y());
            break;
        case Qn::Angle270:
            rotationCenter.setY(rotationCenter.x());
            break;
    }
    rotation = m_fixedRotation;

    setTransformOriginPoint(rotationCenter);
    setRotation(rotation);

    setTransform(QTransform::fromScale(scale, scale));
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
QVariant QnViewportBoundWidget::itemChange(GraphicsItemChange change, const QVariant &value) {
    if(change == ItemSceneHasChanged) {
        if(!scene()) {
            if(m_instrument) {
                disconnect(m_instrument.data(), NULL, this, NULL);
                m_instrument.clear();
            }
        } else {
            QList<InstrumentManager *> managers = InstrumentManager::managersOf(scene());
            if(!managers.empty()) {
                InstrumentManager *manager = managers[0];
                TransformListenerInstrument *instrument = manager->instrument<TransformListenerInstrument>();
                if(!instrument) {
                    qnWarning("An instance of TransformListenerInstrument must be installed in order for the widget to work correctly.");
                } else {
                    m_instrument = instrument;
                    connect(instrument, SIGNAL(transformChanged(QGraphicsView *)), this, SLOT(updateScale(QGraphicsView *)));
                }
            }
        }
    }

    return base_type::itemChange(change, value);
}

void QnViewportBoundWidget::resizeEvent(QGraphicsSceneResizeEvent *event) {
    base_type::resizeEvent(event);
    if(!m_inUpdateScale)
        updateScale();
}
