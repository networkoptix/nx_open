#pragma once

#include <chrono>

#include "abstract_search_widget.h"

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/event_search/models/analytics_search_list_model.h>

namespace nx::vms::client::desktop {

class AnalyticsSearchWidget: public AbstractSearchWidget
{
    Q_OBJECT
    using base_type = AbstractSearchWidget;

public:
    AnalyticsSearchWidget(QnWorkbenchContext* context, QWidget* parent = nullptr);
    virtual ~AnalyticsSearchWidget() override;

    virtual void resetFilters() override;

    QRectF filterRect() const;
    void setFilterRect(const QRectF& value);

    bool areaSelectionEnabled() const;
    QString selectedObjectType() const;

    void setLiveTimestampGetter(AnalyticsSearchListModel::LiveTimestampGetter value);

signals:
    void areaSelectionEnabledChanged(bool value, QPrivateSignal);
    void areaSelectionRequested(QPrivateSignal);
    void filterRectChanged(const QRectF& value);
    void selectedObjectTypeChanged();

private:
    virtual QString placeholderText(bool constrained) const override;
    virtual QString itemCounterText(int count) const override;
    virtual bool calculateAllowance() const override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
