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

class SelectableTextButton: public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(State state READ state WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(bool deactivatable READ deactivatable WRITE setDeactivatable)
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

    bool deactivatable() const;
    void setDeactivatable(bool value);

    // Text for deactivated state. If not set, text() is used instead.
    QString deactivatedText() const;
    void setDeactivatedText(const QString& value);
    void unsetDeactivatedText();

    // Text for current state:
    QString effectiveText() const;

    // Icon for deactivated state. If not set, icon() is used instead.
    QIcon deactivatedIcon() const;
    void setDeactivatedIcon(const QIcon& value);
    void unsetDeactivatedIcon();

    // Icon for current state:
    QIcon effectiveIcon() const;

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
