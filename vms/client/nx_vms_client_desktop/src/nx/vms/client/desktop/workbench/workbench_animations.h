// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <QtCore/QObject>
#include <QtCore/QEasingCurve>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/singleton.h>

class VariantAnimator;
class WidgetAnimator;

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

class Animations: public QObject, public Singleton<Animations>
{
    Q_OBJECT
    using base_type = QObject;
public:
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Id,
        /** Timeline show/hide by button */
        TimelineExpand,
        TimelineCollapse,

        /** Timeline show/hide by opacity change (e.g. in videowall mode). */
        TimelineShow,
        TimelineHide,

        TimelineTooltipShow,
        TimelineTooltipHide,

        SpeedTooltipShow,
        SpeedTooltipHide,

        VolumeTooltipShow,
        VolumeTooltipHide,

        ResourcesPanelExpand,
        ResourcesPanelCollapse,
        ResourcesPanelTooltipShow,
        ResourcesPanelTooltipHide,

        NotificationsPanelExpand,
        NotificationsPanelCollapse,
        RightPanelTileInsertion,
        RightPanelTileRemoval,
        RightPanelTileFadeIn,
        RightPanelTileFadeOut,

        TitlePanelExpand,
        TitlePanelCollapse,

        CalendarShow,
        CalendarHide,

        /**
         * Item size increasing/decreasing on click;
         * New items appearing.
         * Rotating items by context menu option.
         * Restoring item size after resizing.
         * Restoring item position after dragging.
         * Adding new item on scene on double-click.
         */
        SceneItemGeometryChange,

        /**
         * Item goes in fullscreen.
         * Switch between items in fullscreen.
         */
        SceneZoomIn,

        /**
        * Item goes out of fullscreen.
        * Scene size and position restoring on double click on a free space.
        */
        SceneZoomOut,

        ItemOverlayShow,
        ItemOverlayHide,

        IdCount
    );

    Animations(QObject* parent = nullptr);
    virtual ~Animations();

    void setupAnimator(VariantAnimator* animator, Id id);
    void setupAnimator(WidgetAnimator* animator, Id id);

    QEasingCurve::Type easing(Id id) const;
    void setEasing(Id id, QEasingCurve::Type value);

    int timeLimit(Id id) const;
    void setTimeLimit(Id id, int value);

private:
    static const int kIdSize = static_cast<int>(Id::IdCount);
    std::array<QEasingCurve::Type, kIdSize> m_easing;
    std::array<int, kIdSize> m_timeLimit;
};

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop

#define qnWorkbenchAnimations nx::vms::client::desktop::ui::workbench::Animations::instance()
