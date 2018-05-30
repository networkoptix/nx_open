#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QPushButton>

namespace nx {
namespace client {
namespace desktop {

/**
 * A button that can be selected like a tab in a tabbar.
 * It also can be deactivatable and have a clickable [x] control which puts the button
 * into deactivated state with flat appearance and optionally different text and icon.
 * Clicking the button in either deactivated or unselected state puts it into selected state.
 * Unselected state has optional highlighted blue appearance.
 *
 * Palette and icons used in button states:
 *      deactivated: QPalette::Inactive, QIcon::Off
 *          background: none
 *          text (normal): QPalette::WindowText
 *          text (hovered): QPalette::Text
 *          icon (normal): QIcon::Normal
 *          icon (hovered): QIcon::Active
 *
 *      unselected: QPalette::Inactive, QIcon::Off
 *          background (normal, pressed): QPalette::Window
 *          background (hovered): QPalette::Midlight
 *          frame: none
 *          text: QPalette::WindowText
 *          icon: QIcon::Normal
 *
 *      unselected (blue): QPalette::Inactive, QIcon::Off
 *          background (normal, pressed): QPalette::Highlight
 *          background (hovered): QPalette::Light
 *          frame: none
 *          text: QPalette::HighlightedText
 *          icon: QIcon::Selected
 *
 *      selected: QPalette::Active, QIcon::On
 *          background: QPalette::Dark
 *          frame: QPalette::Shadow
 *          text: QPalette::WindowText
 *          icon: QIcon::Normal
 *
 * QPalette::Disabled is never used; semi-opaque painting is used when the button is disabled.
 *
 */
class SelectableTextButton: public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(State state READ state WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(bool deactivatable READ deactivatable WRITE setDeactivatable)
    Q_PROPERTY(bool selectable READ selectable WRITE setSelectable)
    Q_PROPERTY(bool blue READ blue WRITE setBlue)
    Q_PROPERTY(QString deactivatedText READ deactivatedText WRITE setDeactivatedText)

    using base_type = QPushButton;

public:
    explicit SelectableTextButton(QWidget* parent = nullptr);
    virtual ~SelectableTextButton() override;

    enum class State
    {
        unselected,
        selected,
        deactivated
    };

    State state() const;
    void setState(State value);

    // Whether selected state is allowed.
    bool selectable() const;
    void setSelectable(bool value);

    // Whether deactivated state is allowed.
    bool deactivatable() const;
    void setDeactivatable(bool value);

    // Alternative highlighted appearance for unselected state.
    bool blue() const;
    void setBlue(bool blue);

    void deactivate(); //< Helper slot. Equivalent to setState(State::deactivated).

    // Text for deactivated state. If not set, text() is used instead.
    QString deactivatedText() const;
    void setDeactivatedText(const QString& value);
    void unsetDeactivatedText();

    // Text for current state.
    QString effectiveText() const;

    // Icon for deactivated state. If not set, icon() is used instead.
    QIcon deactivatedIcon() const;
    void setDeactivatedIcon(const QIcon& value);
    void unsetDeactivatedIcon();

    // Icon for current state.
    QIcon effectiveIcon() const;

    // Tooltip for deactivation button.
    QString deactivationToolTip() const;
    void setDeactivationToolTip(const QString& value);

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

signals:
    void stateChanged(State state);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual bool event(QEvent* event) override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
