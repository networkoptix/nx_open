// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <qt_graphics_items/graphics_widget.h>

#include <ui/workbench/workbench_context_aware.h>

class QGraphicsLinearLayout;
class QnFramedWidget;
class QnImageButtonWidget;
class QnTimeSlider;
class QnTimeScrollBar;
class QnWorkbenchNavigator;

namespace nx::vms::client::desktop::workbench::timeline {

class ThumbnailPanel;

} // nx::vms::client::desktop::workbench::timeline

class QnNavigationItem : public GraphicsWidget, public nx::vms::client::desktop::WindowContextAware
{
    Q_OBJECT

    using base_type = GraphicsWidget;

public:
    using ThumbnailPanel = nx::vms::client::desktop::workbench::timeline::ThumbnailPanel;

    enum class Zooming
    {
        None,
        In,
        Out,
    };

    explicit QnNavigationItem(QGraphicsItem* parent = nullptr);
    virtual ~QnNavigationItem() override = default;

    QnTimeSlider* timeSlider() const;
    ThumbnailPanel* thumbnailPanel() const;

    void setNavigationRectSize(const QRectF& rect);
    void setControlRectSize(const QRectF& rect);

    Zooming zooming() const;

    virtual bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void navigationPlaceholderGeometryChanged(const QRectF& rect);
    void controlPlaceholderGeometryChanged(const QRectF& rect);

protected:
    virtual void wheelEvent(QGraphicsSceneWheelEvent* event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    virtual void paint(QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

private:
    bool isTimelineRelevant() const;
    void createZoomButton(
        Zooming zooming,
        const QString& zoomingName,
        const QString& statisticsName,
        QGraphicsLinearLayout* layout);
    void updateShadingPos();
    QRectF widgetGeometryMappedToThis(const GraphicsWidget* widget) const;
    void emitNavigationGeometryChanged();
    void emitControlGeometryChanged();

private:
    QnTimeSlider* m_timeSlider = nullptr;
    QGraphicsLinearLayout* m_sliderLayout = nullptr;
    ThumbnailPanel* m_thumbnailPanel = nullptr;
    QnTimeScrollBar* m_timeScrollBar = nullptr;
    QnFramedWidget* m_sliderInnerShading = nullptr;

    GraphicsWidget* m_navigationWidgetPlaceholder = nullptr;
    GraphicsWidget* m_controlWidgetPlaceholder = nullptr;

    Zooming m_zooming = Zooming::None;
};
