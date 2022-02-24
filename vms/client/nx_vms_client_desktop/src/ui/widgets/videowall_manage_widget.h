// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "videowall_model.h"

#include <QtWidgets/QWidget>

#include <client/client_model_types.h>

#include <core/resource/resource_fwd.h>

#include <ui/animation/animation_timer.h>
#include <ui/animation/animation_timer_listener.h>
#include <ui/processors/drag_process_handler.h>

class DragProcessor;
class QAbstractAnimation;

class QnVideowallManageWidgetPrivate;

class QnVideowallManageWidget: public QWidget,
    protected DragProcessHandler,
    protected AnimationTimerListener
{
    Q_OBJECT

    typedef QWidget base_type;
public:
    explicit QnVideowallManageWidget(QWidget* parent = 0);
    virtual ~QnVideowallManageWidget();

    QList<QRect> screenGeometries() const;
    void setScreenGeometries(const QList<QRect>& value);

    nx::vms::client::desktop::videowall::Model model();
    Q_INVOKABLE QString jsonModel();

    void loadFromResource(const QnVideoWallResourcePtr& videowall);
    void submitToResource(const QnVideoWallResourcePtr& videowall);

    virtual QSize minimumSizeHint() const override;
    /** Count of videowall items that is proposed to be on this PC. */
    int proposedItemsCount() const;

signals:
    void itemsChanged();

protected:
    Q_DECLARE_PRIVATE(QnVideowallManageWidget);

    virtual void paintEvent(QPaintEvent* event) override;

    virtual bool eventFilter(QObject* target, QEvent* event) override;

    virtual void startDrag(DragInfo* info) override;
    virtual void dragMove(DragInfo* info) override;
    virtual void finishDrag(DragInfo* info) override;

    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;

    virtual void tick(int deltaTime) override;

private:
    QScopedPointer<QnVideowallManageWidgetPrivate> d_ptr;
    DragProcessor* m_dragProcessor;
    bool m_skipReleaseEvent;
    Qt::MouseButtons m_pressedButtons;
    QList<QRect> m_screenGeometries;
};
