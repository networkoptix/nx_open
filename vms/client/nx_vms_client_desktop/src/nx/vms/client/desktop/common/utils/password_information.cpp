#include "password_information.h"

using namespace nx::utils;

namespace nx::vms::client::desktop {

PasswordInformation::PasswordInformation(
    const QString& text,
    const QString& hint,
    PasswordAcceptance acceptance)
    :
    m_text(text),
    m_hint(hint),
    m_acceptance(acceptance)
{
}

PasswordInformation::PasswordInformation(PasswordStrength strength)
{
    m_acceptance = passwordAcceptance(strength);
    switch (strength)
    {
        case PasswordStrength::Good:
            m_text = tr("Good");
            break;

        case PasswordStrength::Fair:
            m_text = tr("Fair");
            break;

        case PasswordStrength::Weak:
            m_text = tr("Weak");
            m_hint = tr("Password should contain different types of symbols.");
            break;

        case PasswordStrength::WeakAndFair:
            m_text = tr("Weak");
            m_hint = tr("Password should contain %n different combinations of either characters, symbols, or digits",
                "", PasswordLimitations::kPreferredCategories);
            break;

        case PasswordStrength::WeakAndGood:
            m_text = tr("Weak");
            m_hint = tr("Password should contain %n different combinations of either characters, symbols, or digits",
                "", PasswordLimitations::kMinimumCategories);
            break;

        case PasswordStrength::Short:
            m_text = tr("Short");
            m_hint = tr("Password must be at least %n characters long.",
                "", PasswordLimitations::kMinimumLength);
            break;

        case PasswordStrength::Long:
            m_text = tr("Long");
            m_hint = tr("Password must be no longer than %n characters.",
                "", PasswordLimitations::kMaximumLengthForCamera);
            break;

        case PasswordStrength::Conseq:
            m_text = tr("Weak");
            m_hint = tr("Password should not contain %n or more consecutive characters together.",
                "", PasswordLimitations::kConsecutiveCharactersLimit);
            break;

        case PasswordStrength::Repeat:
            m_text = tr("Weak");
            m_hint = tr("Password should not contain %n or more repeating characters.",
                "", PasswordLimitations::kRepeatingCharactersLimit);
            break;

        case PasswordStrength::Common:
            m_text = tr("Common");
            m_hint = tr("This password is in list of the most popular passwords.");
            break;

        case PasswordStrength::IncorrectCamera:
            m_text = tr("Incorrect");
            m_hint = tr("Only latin letters, numbers and keyboard symbols %1 are allowed.")
                .arg(QString::fromLatin1(PasswordLimitations::kCameraAllowedSymbols));
            break;

        case PasswordStrength::Incorrect:
        default:
            m_text = tr("Incorrect");
            m_hint = tr("Only latin letters, numbers and keyboard symbols are allowed.");
            break;
    }
}

PasswordInformation::PasswordInformation(const QString& password, AnalyzeFunction passwordAnalyzer):
    PasswordInformation(passwordAnalyzer(password))
{
}

bool PasswordInformation::operator==(const PasswordInformation& other) const
{
    return m_text == other.m_text && m_hint == other.m_hint && m_acceptance == other.m_acceptance;
}

bool PasswordInformation::operator!=(const PasswordInformation& other) const
{
    return !(*this == other);
}

QString PasswordInformation::text() const
{
    return m_text;
}

QString PasswordInformation::hint() const
{
    return m_hint;
}

utils::PasswordAcceptance PasswordInformation::acceptance() const
{
    return m_acceptance;
}

} // namespace nx::vms::client::desktop
