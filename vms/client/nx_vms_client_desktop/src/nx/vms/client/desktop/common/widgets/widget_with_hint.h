#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include "private/widget_with_hint_p.h"

#include <type_traits>

#include <QtCore/QScopedPointer>

namespace nx::vms::client::desktop {

/**
 * Widget with a question mark that displays a tooltip with specified text upon hover.
 */
template<class Base>
class WidgetWithHint: public Base
{
    static_assert(std::is_base_of<QWidget, Base>::value, "Base must be derived from QWidget");

    using base_type = Base;
    using this_type = WidgetWithHint<Base>;

public:
    using base_type::base_type; //< Forward constructors.

    QString hint() const { return d()->hint(); }
    void setHint(const QString& value) { d()->setHint(value); }
    void addHintLine(const QString& value) { d()->addHintLine(value); }

    int spacing() const { return d()->spacing(); }
    void setSpacing(int value) { d()->setSpacing(value); }

    virtual QSize minimumSizeHint() const override
    {
        return d()->minimumSizeHint(base_type::minimumSizeHint());
    }

protected:
    virtual void resizeEvent(QResizeEvent* event) override
    {
        d()->handleResize();
        base_type::resizeEvent(event);
    }

private:
    WidgetWithHintPrivate* d() const
    {
        if (!m_d) //< Since constructors are forwarded, implementation object is created on demand.
            m_d.reset(new WidgetWithHintPrivate(const_cast<this_type*>(this)));

        return m_d.data();
    }

private:
    mutable QScopedPointer<WidgetWithHintPrivate> m_d;
};

using LabelWithHint = WidgetWithHint<QLabel>;

} // namespace nx::vms::client::desktop
