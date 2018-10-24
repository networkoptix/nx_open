#pragma once

#include <QtCore/QScopedPointer>

#include "abstract_search_widget.h"

namespace nx::client::desktop {

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

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::client::desktop
