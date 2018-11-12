#pragma once

#include <functional>
#include <type_traits>

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/common/utils/abstract_text_provider.h>

namespace nx::vms::client::desktop {

/**
 * This class establishes a link between AbstractTextProvider and a widget - any QWidget descendant
 * that has setText() method - so the widget displays provided text and updates when it changes.
 */
class ProvidedTextDisplay: public QObject
{
    Q_OBJECT

public:
    explicit ProvidedTextDisplay(QObject* parent = nullptr);
    explicit ProvidedTextDisplay(AbstractTextProvider* textProvider, QObject* parent = nullptr);

    AbstractTextProvider* textProvider() const;
    void setTextProvider(AbstractTextProvider* value); //< Doesn't take ownership.

    template<class Widget>
    void setDisplayingWidget(Widget* widget); //< Doesn't take ownership.

private:
    void updateText();
    void setText(const QString& value);

private:
    QPointer<AbstractTextProvider> m_textProvider;
    std::function<void(const QString&)> m_setter = [](const QString&) {};
};

template<class Widget>
void ProvidedTextDisplay::setDisplayingWidget(Widget* widget)
{
    static_assert(std::is_base_of<QWidget, Widget>::value,
        "Widget must be derived from QWidget");

    m_setter =
        [widget = QPointer<Widget>(widget)](const QString& value)
        {
            if (widget)
                widget->setText(value);
        };

    updateText();
}

} // namespace nx::vms::client::desktop
