#pragma once

#include <chrono>

#include <QtCore/QCoreApplication>
#include <QtWidgets/QGraphicsLinearLayout>

class QDateTime;
class GraphicsLabel;

namespace nx::vms::client::desktop {

class TimelineCursorLayout: public QGraphicsLinearLayout
{
    Q_DECLARE_TR_FUNCTIONS(TimelineCursorLayout);

    using base_type = QGraphicsLayout;
    using milliseconds = std::chrono::milliseconds;
 public:
    explicit TimelineCursorLayout();

    virtual ~TimelineCursorLayout() override;

    // Shows "live" instead of time if isLive.
    void setTimeContent(bool isLive, milliseconds pos = milliseconds(0),
        bool showDate = true, bool showHours = true);

private:
    GraphicsLabel* m_tooltipLine1;
    GraphicsLabel* m_tooltipLine2;
};

} // namespace nx::vms::client::desktop
