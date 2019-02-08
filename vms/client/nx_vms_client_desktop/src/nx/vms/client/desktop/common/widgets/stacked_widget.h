#pragma once

#include <QtWidgets/QStackedWidget>

namespace nx::vms::client::desktop {

/**
 * It updates the size according to the current widget.
 * Regular QStackedWidget will fit to the size of the largest widget.
 */
class StackedWidget: public QStackedWidget
{
    Q_OBJECT
    using base_type = QStackedWidget;

public:
    explicit StackedWidget(QWidget* parent = nullptr):
        base_type(parent)
    {

    }

    virtual QSize sizeHint() const override
    {
        auto current = currentWidget();
        if (current)
            return current->sizeHint();
        return base_type::sizeHint();
    }

    virtual QSize minimumSizeHint() const override
    {
        auto current = currentWidget();
        if (current)
            return current->minimumSizeHint();
        return base_type::minimumSizeHint();
    }
};

} // namespace nx::vms::client::desktop
