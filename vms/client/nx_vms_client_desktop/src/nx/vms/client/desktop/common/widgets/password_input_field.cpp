// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "password_input_field.h"
#include "password_strength_indicator.h"
#include "private/password_preview_button.h"

#include <QKeyEvent>

namespace nx::vms::client::desktop {

namespace {

static const QString kPasswordPlaceholder("**********");

} // namespace

struct PasswordInputField::Private
{
    explicit Private(PasswordInputField* q): q(q)
    {
    }

    void updatePasswordIndicatorVisibility()
    {
        if (!passwordIndicator)
            return;

        if (!q->text().trimmed().isEmpty())
            passwordIndicator->setVisible(true);
        else if (hidePasswordIndicatorWhenEmpty)
            passwordIndicator->setVisible(false);
    }

    void updateDisplayedText()
    {
        q->base_type::setText(!storedPassword.isEmpty() || hasRemotePassword
            ? kPasswordPlaceholder
            : QString());
    }

    PasswordInputField* const q;
    PasswordStrengthIndicator* passwordIndicator = nullptr;
    bool hidePasswordIndicatorWhenEmpty = true;
    bool passwordIsLocked = true;
    bool hasRemotePassword = false;
    QString storedPassword;
};

//-------------------------------------------------------------------------------------------------

PasswordInputField::PasswordInputField(QWidget* parent):
    base_type(ValidationBehavior::validateOnFocus, parent),
    d(new Private(this))
{
    const auto lineEdit = qobject_cast<QLineEdit*>(input());
    lineEdit->setObjectName("passwordLineEdit");

    connect(this, &QObject::objectNameChanged, this,
        [lineEdit](const QString& name) { lineEdit->setObjectName(name + "_passwordLineEdit"); });

    lineEdit->setEchoMode(QLineEdit::Password);
    PasswordPreviewButton::createInline(lineEdit, [this]() { return !d->passwordIsLocked; });

    connect(lineEdit, &QLineEdit::textChanged, this,
        [this]()
        {
            d->updatePasswordIndicatorVisibility();
            emit textChanged(text());
        });

    connect(lineEdit, &QLineEdit::textEdited, this, [this]() { d->passwordIsLocked = false; });

    connect(lineEdit,
        &QLineEdit::selectionChanged,
        this,
        [this, lineEdit]()
        {
            if (d->passwordIsLocked
                && lineEdit->hasFocus()
                && lineEdit->selectionLength() < lineEdit->displayText().size())
            {
                // Do not allow a user to change selection while password is locked
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
    const auto lineEdit = qobject_cast<QLineEdit*>(input());
    if (watched == lineEdit && d->passwordIsLocked)
    {
        if (event->type() == QEvent::FocusIn)
        {
            lineEdit->selectAll();
        }
        else if (event->type() == QEvent::FocusOut)
        {
            lineEdit->deselect();
        }
    }
    return base_type::eventFilter(watched, event);
}

ValidationResult PasswordInputField::calculateValidationResult() const
{
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
    return d->passwordIsLocked ? d->storedPassword : base_type::text();
}

void PasswordInputField::setText(const QString& value)
{
    d->storedPassword = value;
    d->passwordIsLocked = true;
    d->updateDisplayedText();
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
    d->updateDisplayedText();
}

} // namespace nx::vms::client::desktop
