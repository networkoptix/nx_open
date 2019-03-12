#pragma once

#include "abstract_search_widget.h"

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class SelectableTextButton;

class EventSearchWidget: public AbstractSearchWidget
{
    Q_OBJECT
    using base_type = AbstractSearchWidget;

public:
    EventSearchWidget(QnWorkbenchContext* context, QWidget* parent = nullptr);
    virtual ~EventSearchWidget() override;

    virtual void resetFilters() override;

private:
    virtual QString placeholderText(bool constrained) const override;
    virtual QString itemCounterText(int count) const override;
    virtual bool calculateAllowance() const override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
