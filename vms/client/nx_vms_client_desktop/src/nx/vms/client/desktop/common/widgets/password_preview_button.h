#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

namespace nx::vms::client::desktop {

/**
 * A button that allows previewing of a password entered into a line edit,
 * i.e. dynamically changing line edit echo mode between Password and Normal,
 * when pressed, hovered or toggled, depending on chosen activation mode.
 */
class PasswordPreviewButton: public QPushButton
{
    Q_OBJECT
    using base_type = QPushButton;

public:
    explicit PasswordPreviewButton(QWidget* parent = nullptr);
    virtual ~PasswordPreviewButton() override = default;

    /** Controlled line edit. Ownership is not affected. */
    QLineEdit* lineEdit() const;
    void setLineEdit(QLineEdit* value);

    enum class ActivationMode
    {
        press,
        hover,
        toggle
    };

    ActivationMode activationMode() const;
    void setActivationMode(ActivationMode value);

    bool isActivated() const;

    /** If the button is visible always or only when controlled line edit has non-empty text. */
    bool alwaysVisible() const;
    void setAlwaysVisible(bool value);

    /** Creates password preview button as an inline action button of specified line edit. */
    static PasswordPreviewButton* createInline(QLineEdit* parent);

protected:
    virtual bool event(QEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;

private:
    void updateEchoMode();
    void updateVisibility();

private:
    QPointer<QLineEdit> m_lineEdit;
    ActivationMode m_activationMode = ActivationMode::press;
    bool m_alwaysVisible = false;
};

} // namespace nx::vms::client::desktop
