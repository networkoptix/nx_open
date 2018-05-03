#include "password_information.h"

using namespace nx::utils;

namespace nx {
namespace client {
namespace desktop {

PasswordInformation::PasswordInformation(
    const QString& text,
    const QString& hint,
    Acceptance acceptance)
    :
    m_text(text),
    m_hint(hint),
    m_acceptance(acceptance)
{
}

PasswordInformation::PasswordInformation(PasswordStrength strength)
{
    switch (strength)
    {
        case PasswordStrength::Good:
            m_text = tr("Good");
            m_acceptance = Good;
            break;

        case PasswordStrength::Fair:
            m_text = tr("Fair");
            m_acceptance = Acceptable;
            break;

        case PasswordStrength::Weak:
            m_text = tr("Weak");
            m_hint = tr("Password should contain different types of symbols.");
            m_acceptance = Inacceptable;
            break;

        case PasswordStrength::WeakAndFair:
            m_text = tr("Weak");
            m_hint = tr("Password should contain %n different combinations of either characters, symbols, or digits",
                "", PasswordLimitations::kPreferredCategories);
            m_acceptance = Inacceptable;
            break;

        case PasswordStrength::WeakAndGood:
            m_text = tr("Weak");
            m_hint = tr("Password should contain %n different combinations of either characters, symbols, or digits",
                "", PasswordLimitations::kMinimumCategories);
            m_acceptance = Inacceptable;
            break;

        case PasswordStrength::Short:
            m_text = tr("Short");
            m_hint = tr("Password must be at least %n characters long.",
                "", PasswordLimitations::kMinimumLength);
            m_acceptance = Inacceptable;
            break;

        case PasswordStrength::Long:
            m_text = tr("Long");
            m_hint = tr("Password must be no longer than %n characters.",
                "", PasswordLimitations::kMaximumLengthForCamera);
            m_acceptance = Inacceptable;
            break;

        case PasswordStrength::Conseq:
            m_text = tr("Weak");
            m_hint = tr("Password should not contain %n or more consecutive characters together.",
                "", PasswordLimitations::kConsecutiveCharactersLimit);
            m_acceptance = Inacceptable;
            break;

        case PasswordStrength::Repeat:
            m_text = tr("Weak");
            m_hint = tr("Password should not contain %n or more repeating characters.",
                "", PasswordLimitations::kRepeatingCharactersLimit);
            m_acceptance = Inacceptable;
            break;

        case PasswordStrength::Common:
            m_text = tr("Common");
            m_hint = tr("This password is in list of the most popular passwords.");
            m_acceptance = Inacceptable;
            break;

        case PasswordStrength::IncorrectCamera:
            m_text = tr("Incorrect");
            m_hint = tr("Only latin letters, numbers and keyboard symbols %1 are allowed.")
                .arg(QString::fromLatin1(PasswordLimitations::kCameraAllowedSymbols));
            m_acceptance = Inacceptable;
            break;

        case PasswordStrength::Incorrect:
        default:
            m_text = tr("Incorrect");
            m_hint = tr("Only latin letters, numbers and keyboard symbols are allowed.");
            m_acceptance = Inacceptable;
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

PasswordInformation::Acceptance PasswordInformation::acceptance() const
{
    return m_acceptance;
}

} // namespace desktop
} // namespace client
} // namespace nx
