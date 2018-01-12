#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QPushButton>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

// A button that can be selected like a tab in a tabbar.
// It also can be deactivatable and have a clickable [x] control which puts the button
// into deactivated state with flat appearance and optionally different text and icon.
// Clicking the button in either deactivated or unselected state puts it into selected state.
//
// Palette groups used in button states:
//     deactivated and unselected: QPalette::Inactive
//     selected: QPalette::Active
//
// QPalette::Disabled is never used; semi-opaque painting is used when the button is disabled.
//
// Icon modes and states used in button states:
//     deactivated: QIcon::Off, QIcon::Normal or QIcon::Active (hovered)
//     unselected: QIcon::Off, QIcon::Normal
//     selected: QIcon::On, QIcon::Normal

class SelectableTextButton: public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(State state READ state WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(bool deactivatable READ deactivatable WRITE setDeactivatable)
    Q_PROPERTY(bool selectable READ selectable WRITE setSelectable)
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

    bool selectable() const;
    void setSelectable(bool value);

    bool deactivatable() const;
    void setDeactivatable(bool value);

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
    void updateDeactivateButtonPalette();

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
