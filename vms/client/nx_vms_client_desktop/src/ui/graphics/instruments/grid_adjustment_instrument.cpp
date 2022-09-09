// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "grid_adjustment_instrument.h"

#include <QtGui/QWheelEvent>
#include <QtWidgets/QGraphicsSceneWheelEvent>

#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_layout.h>

using namespace nx::vms::client::desktop;

GridAdjustmentInstrument::GridAdjustmentInstrument(Workbench *workbench, QObject *parent):
    Instrument(
        makeSet(QEvent::Wheel),
        makeSet(),
        makeSet(QEvent::GraphicsSceneWheel),
        makeSet(),
        parent
    ),
    m_workbench(workbench),
    m_speed(1.0),
    m_maxSpacing(1.0)
{
    NX_ASSERT(workbench != nullptr);
}

GridAdjustmentInstrument::~GridAdjustmentInstrument() {
    return;
}

bool GridAdjustmentInstrument::wheelEvent(QWidget *viewport, QWheelEvent *event) {
    if(event->modifiers() & Qt::ControlModifier) {
        m_currentViewport = viewport;
    } else {
        m_currentViewport.clear();
    }
    return false;
}

qreal GridAdjustmentInstrument::maxSpacing() const
{
    return m_maxSpacing;
}

void GridAdjustmentInstrument::setMaxSpacing(qreal spacing)
{
    m_maxSpacing = spacing;
}

qreal GridAdjustmentInstrument::speed() const
{
    return m_speed;
}

void GridAdjustmentInstrument::setSpeed(qreal speed)
{
    m_speed = speed;
}


bool GridAdjustmentInstrument::wheelEvent(
    QGraphicsScene* /* scene */,
    QGraphicsSceneWheelEvent* event)
{
    QWidget *viewport = m_currentViewport.data();
    if(viewport == nullptr)
        return false;

    /* delta() returns the distance that the wheel is rotated
     * in eighths (1/8s) of a degree. */
    qreal degrees = event->delta() / 8.0;

    if (NX_ASSERT(workbench()))
    {
        auto layout = workbench()->currentLayoutResource();
        const qreal spacing = layout->cellSpacing();
        const qreal delta = m_speed * -degrees;

        qreal k = 1.0;
        if (!qFuzzyIsNull(delta))
        {
            if (delta < 0)
                k = qMin(k, spacing / -delta);
            else
                k = qMin(k, (m_maxSpacing - spacing) / delta);
        }

        layout->setCellSpacing(spacing + k * delta);

        static const auto kNoDelta = QPoint(0, 0);
        moveViewportScene(this->view(viewport), kNoDelta);
    }

    event->accept();
    return false;
}

Workbench* GridAdjustmentInstrument::workbench() const
{
    return m_workbench.data();
}
