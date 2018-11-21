#pragma once

#include <QtCore/QScopedPointer>

#include "abstract_search_widget.h"

class QnMediaResourceWidget;

namespace nx::vms::client::desktop {

class AnalyticsSearchWidget: public AbstractSearchWidget
{
    Q_OBJECT
    using base_type = AbstractSearchWidget;

public:
    AnalyticsSearchWidget(QnWorkbenchContext* context, QWidget* parent = nullptr);
    virtual ~AnalyticsSearchWidget() override;

    void setCurrentMediaWidget(QnMediaResourceWidget* value);

private:
    virtual QString placeholderText(bool constrained) const override;
    virtual QString itemCounterText(int count) const override;

    virtual void resetFilters() override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
