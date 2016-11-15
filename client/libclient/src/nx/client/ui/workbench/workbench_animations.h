#pragma once

#include <array>

#include <QtCore/QObject>
#include <QtCore/QEasingCurve>

#include <nx/utils/singleton.h>

class VariantAnimator;
class WidgetAnimator;

namespace nx {
namespace client {
namespace ui {
namespace workbench {

class Animations: public QObject, public Singleton<Animations>
{
    Q_OBJECT
    using base_type = QObject;
public:
    enum class Id
    {
        TimelineExpanding,
        TimelineCollapsing,
        TimelineTooltipShow,
        TimelineTooltipHide,
        ResourcesPanelExpanding,
        ResourcesPanelCollapsing,
        NotificationsPanelExpanding,
        NotificationsPanelCollapsing,
        CalendarExpanding,
        CalendarCollapsing,

        /**
         * Item size increasing/decreasing on click;
         * New items appearing.
         * Rotating items by context menu option.
         */
        SceneItemGeometryChange,

        /**
         * Item goes in fullscreen.
         * Also handles switch between items in fullscreen.
         */
        SceneZoomIn,

        /**
        * Item goes out of fullscreen.
        */
        SceneZoomOut,

        IdCount
    };

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
} // namespace client
} // namespace nx

#define qnWorkbenchAnimations nx::client::ui::workbench::Animations::instance()
