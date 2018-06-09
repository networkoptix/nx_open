#pragma once

#include <QtCore/QCoreApplication>
#include <QtWidgets/QGraphicsLinearLayout>

class QDateTime;
class GraphicsLabel;

namespace nx {
namespace client {
namespace desktop {

class TimelineCursorLayout: public QGraphicsLinearLayout
{
    Q_DECLARE_TR_FUNCTIONS(TimelineCursorLayout);

    using base_type = QGraphicsLayout;

 public:
    explicit TimelineCursorLayout();

    virtual ~TimelineCursorLayout() override;

    // Shows "live" instead of time if isLive.
    void setTimeContent(bool isLive, qint64 pos = 0, bool showDate = true, bool showHours = true);

private:
    GraphicsLabel* m_tooltipLine1;
    GraphicsLabel* m_tooltipLine2;
};

} // namespace desktop
} // namespace client
} // namespace nx
