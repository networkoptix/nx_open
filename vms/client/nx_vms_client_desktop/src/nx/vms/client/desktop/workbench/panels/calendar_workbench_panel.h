// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/workbench/panels/abstract_workbench_panel.h>

namespace nx::vms::client::desktop {

class TimelineCalendarWidget;

class CalendarWorkbenchPanel: public AbstractWorkbenchPanel
{
public:
    CalendarWorkbenchPanel(
        const QnPaneSettings& settings,
        QWidget* parentwidget,
        QGraphicsWidget* parentGraphicsWidget,
        QObject* parent = nullptr);

    TimelineCalendarWidget* widget() const;

    bool isEnabled() const;
    void setEnabled(bool enabled, bool animated);

    QPoint position() const;
    void setPosition(const QPoint& position);

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

private:
    TimelineCalendarWidget* m_widget = nullptr;
};

} // namespace nx::vms::client::desktop
