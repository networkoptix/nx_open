// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>

#include "abstract_search_widget.h"

namespace nx::vms::client::desktop {

class BookmarkSearchWidget: public AbstractSearchWidget
{
    Q_OBJECT
    using base_type = AbstractSearchWidget;

public:
    BookmarkSearchWidget(WindowContext* context, QWidget* parent = nullptr);
    virtual ~BookmarkSearchWidget() override;

    virtual void resetFilters() override;

private:
    virtual QString placeholderText(bool constrained) const override;
    virtual QString itemCounterText(int count) const override;
    virtual bool calculateAllowance() const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
