#pragma once

#include "abstract_search_widget.h"

namespace nx::client::desktop {

class BookmarkSearchWidget: public AbstractSearchWidget
{
    Q_OBJECT
    using base_type = AbstractSearchWidget;

public:
    BookmarkSearchWidget(QnWorkbenchContext* context, QWidget* parent = nullptr);
    virtual ~BookmarkSearchWidget() override = default;

private:
    virtual QString placeholderText(bool constrained) const override;
    virtual QString itemCounterText(int count) const override;
};

} // namespace nx::client::desktop
