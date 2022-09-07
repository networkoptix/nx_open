// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPoint>
#include <QtWidgets/QPushButton>

#include "abstract_workbench_panel.h"
#include <nx/vms/client/desktop/common/utils/custom_painted.h>

class QGraphicsItem;
class QnCalendarWidget;
class QnImageButtonWidget;
class QnDayTimeWidget;
class VariantAnimator;
class AnimatorGroup;
class HoverFocusProcessor;

namespace nx::vms::client::desktop {

class QnIconButton;

class CalendarWorkbenchPanel: public AbstractWorkbenchPanel
{
    using base_type = AbstractWorkbenchPanel;

    Q_OBJECT

public:
    CalendarWorkbenchPanel(
        const QnPaneSettings& settings,
        QWidget* widget,
        QGraphicsWidget* parentGraphicsWidget,
        QObject* parent = nullptr);

    /** Hover processor that is used to hide the panel when the mouse leaves it. */
    HoverFocusProcessor* hidingProcessor;

public:
    bool isEnabled() const;
    void setEnabled(bool enabled, bool animated);
    QPoint origin() const;
    void setOrigin(const QPoint& position);

    virtual bool isPinned() const override;

    virtual bool isOpened() const override;
    virtual void setOpened(bool opened = true, bool animate = true) override;

    virtual bool isVisible() const override;
    virtual void setVisible(bool visible = true, bool animate = true) override;

    virtual qreal opacity() const override;
    virtual void setOpacity(qreal opacity, bool animate = true) override;

    virtual bool isHovered() const override;

    virtual void setPanelSize(qreal size) override;
    virtual QRectF effectiveGeometry() const override;
    virtual QRectF geometry() const override;

    virtual void stopAnimations() override;

    void setDayTimeWidgetOpened(bool opened = true);

    // TODO: #sivanov Probably make this AbstractWorkbenchPanel interface function.
    QList<QWidget*> activeWidgets() const;

private:
    void updateControlsGeometry();

private:
    bool m_ignoreClickEvent;
    bool m_visible;

    QPoint m_origin;

    QnCalendarWidget* m_widget;
    QnDayTimeWidget* m_dayTimeWidget;

    using CustomPaintedButton = nx::vms::client::desktop::CustomPainted<QPushButton>;

    CustomPaintedButton* m_pinButton;
    CustomPaintedButton* m_dayTimeMinimizeButton;

    VariantAnimator* m_yAnimator;

    const QPoint m_pinOffset;

    bool m_dayTimeOpened;

    const QPoint m_dayTimeOffset;

    /** Hover processor that is used to change panel opacity when mouse hovers over it. */
    HoverFocusProcessor* m_opacityProcessor;
};

} //namespace nx::vms::client::desktop
