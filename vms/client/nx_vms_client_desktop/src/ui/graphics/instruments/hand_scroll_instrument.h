// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "drag_processing_instrument.h"

#include <common/common_globals.h>
#include <ui/processors/kinetic_process_handler.h>

class HandScrollInstrument:
    public DragProcessingInstrument,
    protected KineticProcessHandler
{
    Q_OBJECT
    using base_type = DragProcessingInstrument;

public:
    explicit HandScrollInstrument(QObject* parent);
    virtual ~HandScrollInstrument() override;

    Qt::MouseButtons mouseButtons() const;
    void setMouseButtons(Qt::MouseButtons mouseButtons);

    void emulate(QPoint viewportDelta);

signals:
    void scrollProcessStarted(QGraphicsView* view);
    void scrollStarted(QGraphicsView* view);
    void scrollFinished(QGraphicsView* view);
    void scrollProcessFinished(QGraphicsView* view);

protected:
    virtual void aboutToBeDisabledNotify() override;

    virtual bool mousePressEvent(QWidget* viewport, QMouseEvent* event) override;

    virtual void startDragProcess(DragInfo* info) override;
    virtual void startDrag(DragInfo* info) override;
    virtual void dragMove(DragInfo* info) override;
    virtual void finishDrag(DragInfo* info) override;
    virtual void finishDragProcess(DragInfo* info) override;

    virtual void kineticMove(const QVariant& distance) override;
    virtual void finishKinetic() override;

private:
    Qt::MouseButtons m_mouseButtons;
    QPointer<QGraphicsView> m_currentView;
};

/**
 * Name of the property to set on a graphics object to forbid hand scrolling
 * on top of it. By default, this property is not set, and hand scrolling is
 * thus enabled.
 */
#define NoHandScrollOver _id("_qn_noHandScrollOver")
