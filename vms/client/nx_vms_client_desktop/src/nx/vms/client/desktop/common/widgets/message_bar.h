#pragma once

#include <QtWidgets/QWidget>

class QLabel;
class QHBoxLayout;

namespace nx::vms::client::desktop {

class MessageBar: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)

    using base_type = QWidget;

public:
    explicit MessageBar(QWidget* parent = nullptr);

    QString text() const;
    void setText(const QString& text);

    /**
     * This property controls how the bar should behave when no text is set.
     * If reservedSpace is set, bar will be displayed without text and color - just take space.
     * It is used when we don't want to change the dialog size.
     */
    bool reservedSpace() const;
    void setReservedSpace(bool reservedSpace);

    QHBoxLayout* getOverlayLayout();

private:
    void updateVisibility();

private:
    QLabel* const m_label = nullptr;
    QHBoxLayout* const m_layout = nullptr;
    bool m_reservedSpace = false;
};

// The following classes are helpers for easy customization.

class AlertBar: public MessageBar
{
    Q_OBJECT
    using base_type = MessageBar;

public:
    using base_type::base_type; //< Forward constructors.
};

class PromoBar: public MessageBar
{
    Q_OBJECT
    using base_type = MessageBar;

public:
    using base_type::base_type; //< Forward constructors.
};

} // namespace nx::vms::client::desktop
