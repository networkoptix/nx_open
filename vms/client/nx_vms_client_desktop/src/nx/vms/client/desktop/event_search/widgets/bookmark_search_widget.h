#pragma once

#include <QtCore/QScopedPointer>

#include "abstract_search_widget.h"

namespace nx::vms::client::desktop {

class BookmarkSearchWidget: public AbstractSearchWidget
{
    Q_OBJECT
    using base_type = AbstractSearchWidget;

public:
    BookmarkSearchWidget(QnWorkbenchContext* context, QWidget* parent = nullptr);
    virtual ~BookmarkSearchWidget() override;

private:
    virtual QString placeholderText(bool constrained) const override;
    virtual QString itemCounterText(int count) const override;
    virtual bool calculateAllowance() const override;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
