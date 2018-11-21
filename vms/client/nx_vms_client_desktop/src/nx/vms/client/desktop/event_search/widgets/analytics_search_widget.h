#pragma once

#include "abstract_search_widget.h"

namespace nx::vms::client::desktop {

class AnalyticsSearchWidget: public AbstractSearchWidget
{
    Q_OBJECT
    using base_type = AbstractSearchWidget;

public:
    AnalyticsSearchWidget(QnWorkbenchContext* context, QWidget* parent = nullptr);
    virtual ~AnalyticsSearchWidget() override = default;

    void setFilterRect(const QRectF& value);

private:
    virtual QString placeholderText(bool constrained) const override;
    virtual QString itemCounterText(int count) const override;

    virtual void resetFilters() override;

    void updateTimelineDisplay();
};

} // namespace nx::vms::client::desktop
