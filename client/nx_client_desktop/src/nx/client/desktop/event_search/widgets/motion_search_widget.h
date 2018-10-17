#pragma once

#include <QtCore/QList>
#include <QtGui/QRegion>

#include "abstract_search_widget.h"

namespace nx::client::desktop {

class MotionSearchWidget: public AbstractSearchWidget
{
    Q_OBJECT
    using base_type = AbstractSearchWidget;

public:
    MotionSearchWidget(QnWorkbenchContext* context, QWidget* parent = nullptr);
    virtual ~MotionSearchWidget() override = default;

    void setFilterArea(const QList<QRegion>& value);

private:
    virtual QString placeholderText(bool constrained) const override;
    virtual QString itemCounterText(int count) const override;

    void updateTimelineDisplay();
};

} // namespace nx::client::desktop
