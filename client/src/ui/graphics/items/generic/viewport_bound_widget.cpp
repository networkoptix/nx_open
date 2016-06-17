#include "viewport_bound_widget.h"

#include <cmath> /* For std::sqrt. */

#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsScale>

#include <nx/utils/math/fuzzy.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/warnings.h>

#include <ui/graphics/instruments/transform_listener_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>


QnViewportBoundWidget::QnViewportBoundWidget(QGraphicsItem *parent):
    base_type(parent),
    m_fixedSize(QSizeF(0.0, 0.0)),
    m_inUpdateScale(false),
    m_lastView(nullptr)
{
    m_scale = new QGraphicsScale(this);
    QList<QGraphicsTransform *> transformations = this->transformations();
    transformations.push_back(m_scale);
    setTransformations(transformations);

    itemChange(ItemSceneHasChanged, QVariant::fromValue(scene()));
}

QnViewportBoundWidget::~QnViewportBoundWidget() {
    return;
}

const QSizeF &QnViewportBoundWidget::fixedSize() {
    return m_fixedSize;
}

void QnViewportBoundWidget::setFixedSize(const QSizeF &fixedSize) {
    if(qFuzzyEquals(m_fixedSize, fixedSize))
        return;

    m_fixedSize = fixedSize;

    updateScale();
}

void QnViewportBoundWidget::updateScale(QGraphicsView *view) {
    if (m_fixedSize.isNull())
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

    QN_SCOPED_VALUE_ROLLBACK(&m_inUpdateScale, true);

    /* Assume affine transform that does not change x/y scale separately. */
    QTransform sceneToViewport = view->viewportTransform();
    qreal scale = 1.0 / std::sqrt(sceneToViewport.m11() * sceneToViewport.m11() + sceneToViewport.m12() * sceneToViewport.m12());

    QRectF geometry = QRectF(QPointF(0, 0), m_fixedSize / scale);
    setGeometry(geometry);

    QSizeF resultingSize = size();
    if(resultingSize.width() > geometry.width() || resultingSize.height() > geometry.height()) {
        qreal k;
        if(qFuzzyIsNull(geometry.width()) || qFuzzyIsNull(geometry.height())) {
            k = 1.0e6; 
        } else {
            k = qMax(resultingSize.width() / geometry.width(), resultingSize.height() / geometry.height());
        }

        geometry.setSize(geometry.size() * k);
        scale /= k;

        setGeometry(geometry);
    }

    m_scale->setXScale(scale);
    m_scale->setYScale(scale);

    emit scaleUpdated(m_lastView, scale);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
QVariant QnViewportBoundWidget::itemChange(GraphicsItemChange change, const QVariant &value) {
    if(change == ItemSceneHasChanged) {
        if(m_instrument) {
            disconnect(m_instrument.data(), NULL, this, NULL);
            m_instrument.clear();
        }

        if(InstrumentManager *manager = InstrumentManager::instance(scene())) {
            TransformListenerInstrument *instrument = manager->instrument<TransformListenerInstrument>();
            if(!instrument) {
                qnWarning("An instance of TransformListenerInstrument must be installed in order for the widget to work correctly.");
            } else {
                m_instrument = instrument;
                connect(instrument, SIGNAL(transformChanged(QGraphicsView *)), this, SLOT(updateScale(QGraphicsView *)));

                updateScale();
            }
        }
    }

    return base_type::itemChange(change, value);
}

void QnViewportBoundWidget::setGeometry(const QRectF &geometry) {
    if(m_inUpdateScale) {
        base_type::setGeometry(geometry);
    } else {
        QSizeF oldSize = size();

        base_type::setGeometry(geometry);

        if(!qFuzzyEquals(size(), oldSize))
            updateScale();
    }
}

