#pragma once

#include "abstract_search_widget.h"

namespace nx::client::desktop {

class AnalyticsSearchWidget: public AbstractSearchWidget
{
    Q_OBJECT
    using base_type = AbstractSearchWidget;

public:
    AnalyticsSearchWidget(QnWorkbenchContext* context, QWidget* parent = nullptr);
    virtual ~AnalyticsSearchWidget() override = default;

private:
    virtual QString placeholderText(bool constrained) const override;
    virtual QString itemCounterText(int count) const override;
};

} // namespace nx::client::desktop
