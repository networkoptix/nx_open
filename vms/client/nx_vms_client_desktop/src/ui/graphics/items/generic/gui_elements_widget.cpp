#include "gui_elements_widget.h"

#include <QtWidgets/QGraphicsView>

#include <utils/common/warnings.h>

#include <ui/graphics/instruments/transform_listener_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>

QnGuiElementsWidget::QnGuiElementsWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags) 
{
    itemChange(ItemSceneHasChanged, QVariant::fromValue(this->scene()));
    itemChange(ItemParentHasChanged, QVariant::fromValue(this->parent()));
}

QnGuiElementsWidget::~QnGuiElementsWidget() {
    return;
}

QVariant QnGuiElementsWidget::itemChange(GraphicsItemChange change, const QVariant &value) {
    switch(change) {
    case ItemParentHasChanged:
        if(parentItem())
            qnWarning("QnUiElementsWidget is not supposed to have a parent item.");
        break;
    case ItemSceneHasChanged:
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
                connect(instrument, SIGNAL(transformChanged(QGraphicsView *)), this, SLOT(updateTransform(QGraphicsView *)));
                connect(instrument, SIGNAL(sizeChanged(QGraphicsView *)), this, SLOT(updateSize(QGraphicsView *)));

                if(!manager->views().isEmpty()) {
                    QGraphicsView *view = *manager->views().begin();
                    updateSize(view);
                    updateTransform(view);
                }
            }
        }
        break;
    default:
        break;
    }

    return base_type::itemChange(change, value);
}

void QnGuiElementsWidget::updateTransform(QGraphicsView *view) {
    setTransform(view->viewportTransform().inverted());
}

void QnGuiElementsWidget::updateSize(QGraphicsView *view) {
    resize(view->viewport()->size());
}
