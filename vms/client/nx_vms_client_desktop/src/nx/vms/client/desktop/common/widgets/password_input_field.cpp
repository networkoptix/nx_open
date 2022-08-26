// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtGui/QKeyEvent>
#include <QtWidgets/QStyle>

#include "password_input_field.h"
#include "password_strength_indicator.h"
#include "private/password_preview_button.h"

namespace nx::vms::client::desktop {

namespace {

QString passwordPlaceholder(QStyle* style)
{
    static constexpr auto kPlaceholderLength = 10;
    const auto passwordPlaceholderChar = style->styleHint(QStyle::SH_LineEdit_PasswordCharacter);
    return QString(kPlaceholderLength, passwordPlaceholderChar);
}

} // namespace

struct PasswordInputField::Private
{
    void updatePasswordIndicatorVisibility()
    {
        if (!passwordIndicator)
            return;

        if (hasRemotePassword)
        {
            passwordIndicator->setVisible(false);
            return;
        }

        if (!q->text().trimmed().isEmpty())
            passwordIndicator->setVisible(true);
        else if (hidePasswordIndicatorWhenEmpty)
            passwordIndicator->setVisible(false);
    }

    PasswordInputField* const q;
    PasswordStrengthIndicator* passwordIndicator = nullptr;
    bool hidePasswordIndicatorWhenEmpty = true;
    bool hasRemotePassword = false;
};

//-------------------------------------------------------------------------------------------------

PasswordInputField::PasswordInputField(QWidget* parent):
    base_type(ValidationBehavior::validateOnFocus, parent),
    d(new Private({this}))
{
    const auto lineEdit = qobject_cast<QLineEdit*>(input());
    lineEdit->setObjectName("passwordLineEdit");

    connect(this, &QObject::objectNameChanged, this,
        [lineEdit](const QString& name) { lineEdit->setObjectName(name + "_passwordLineEdit"); });

    lineEdit->setEchoMode(QLineEdit::Password);

    PasswordPreviewButton::createInline(lineEdit, [this]() { return !hasRemotePassword(); });

    connect(lineEdit, &QLineEdit::textChanged, this,
        [this](const QString& text) { d->updatePasswordIndicatorVisibility(); });

    connect(lineEdit, &QLineEdit::textEdited, this,
        [this]() { setHasRemotePassword(false); });

    connect(lineEdit, &QLineEdit::selectionChanged, this,
        [this, lineEdit]()
        {
            if (hasRemotePassword()
                && lineEdit->hasFocus()
                && lineEdit->selectionLength() < lineEdit->displayText().size())
            {
                // Do not allow a user to change selection while control displays the presence
                // of a remote password.
                lineEdit->selectAll();
            }
        });

    lineEdit->installEventFilter(this);
}

PasswordInputField::~PasswordInputField()
{
    // Required here for forward-declared scoped pointer destruction.
}

bool PasswordInputField::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == lineEdit() && hasRemotePassword())
    {
        if (event->type() == QEvent::FocusIn)
            lineEdit()->selectAll();

        if (event->type() == QEvent::FocusOut)
            lineEdit()->deselect();
    }

    return base_type::eventFilter(watched, event);
}

ValidationResult PasswordInputField::calculateValidationResult() const
{
    if (hasRemotePassword())
        return ValidationResult(QValidator::Acceptable);

    auto result = base_type::calculateValidationResult();

    if ((result.state == QValidator::Acceptable) && d->passwordIndicator
        && d->passwordIndicator->isVisible())
    {
        const auto& info = d->passwordIndicator->currentInformation();
        if (info.acceptance() == utils::PasswordAcceptance::Unacceptable)
            result = ValidationResult(info.hint());
    }

    return result;
}

void PasswordInputField::setPasswordIndicatorEnabled(
    bool enabled,
    bool hideForEmptyInput,
    bool showImmediately,
    PasswordInformation::AnalyzeFunction analyzeFunction)
{
    bool update = d->hidePasswordIndicatorWhenEmpty != hideForEmptyInput;
    d->hidePasswordIndicatorWhenEmpty = hideForEmptyInput;

    if (enabled)
    {
        if (!d->passwordIndicator)
        {
            // LineEditControls will take ownership over passwordIndicator
            d->passwordIndicator = new PasswordStrengthIndicator(lineEdit(), analyzeFunction);
            d->passwordIndicator->setVisible(false);
        }

        if (update || showImmediately)
            d->updatePasswordIndicatorVisibility();
    }
    else
    {
        delete d->passwordIndicator;
        d->passwordIndicator = nullptr;
    }
}

QString PasswordInputField::text() const
{
    return hasRemotePassword()
        ? QString()
        : base_type::text();
}

void PasswordInputField::setText(const QString& value)
{
    setHasRemotePassword(false);
    base_type::setText(value);
}

bool PasswordInputField::hasRemotePassword() const
{
    return d->hasRemotePassword;
}

void PasswordInputField::setHasRemotePassword(bool value)
{
    if (d->hasRemotePassword == value)
        return;

    d->hasRemotePassword = value;

    if (d->hasRemotePassword)
    {
        if (!base_type::text().isEmpty())
            emit textChanged(QString());

        lineEdit()->clearFocus();

        QSignalBlocker textChangedSignalBlocker(this);
        base_type::setText(passwordPlaceholder(style()));
    }
    else if (base_type::text() == passwordPlaceholder(style()))
    {
        QSignalBlocker textChangedSignalBlocker(this);
        base_type::setText(QString());
    }
}

} // namespace nx::vms::client::desktop
