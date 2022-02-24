// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

namespace nx::vms::client::desktop {

/**
 * A button that allows previewing of a password entered into a line edit,
 * i.e. dynamically changing line edit echo mode between Password and Normal,
 * when toggled.
 */
class PasswordPreviewButton: public QPushButton
{
    Q_OBJECT
    using base_type = QPushButton;
    using Validator = std::function<bool()>;

public:
    explicit PasswordPreviewButton(QWidget* parent = nullptr);
    virtual ~PasswordPreviewButton() override = default;

    /** Controlled line edit. Ownership is not affected. */
    QLineEdit* lineEdit() const;
    void setLineEdit(QLineEdit* value);

    bool isActivated() const;

    /** If the button is visible always or only when controlled line edit has non-empty text. */
    bool alwaysVisible() const;
    void setAlwaysVisible(bool value);

    /**
     *  Creates password preview button as an inline action button of specified line edit.
     *  `visibilityCondition` is used as additional button visibility condition.
     */
    static PasswordPreviewButton* createInline(
        QLineEdit* parent, Validator visibilityCondition = nullptr);

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    void updateEchoMode();
    void updateVisibility();
    void setVisibilityCondition(Validator visibilityCondition);

private:
    QPointer<QLineEdit> m_lineEdit;
    bool m_alwaysVisible = false;
    Validator m_visibilityCondition = nullptr;
};

} // namespace nx::vms::client::desktop
