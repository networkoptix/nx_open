#include "password_information.h"

using namespace nx::utils;

QnPasswordInformation::QnPasswordInformation() : m_acceptance(Inacceptable)
{
}

QnPasswordInformation::QnPasswordInformation(const QString& text, const QString& hint, Acceptance acceptance) :
    m_text(text), m_hint(hint), m_acceptance(acceptance)
{
}

QnPasswordInformation::QnPasswordInformation(PasswordStrength strength)
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

        case PasswordStrength::Short:
            m_text = tr("Short");
            m_hint = tr("Password must be at least %1 characters long.")
                .arg(PasswordLimitations::kMinimumLength);
            m_acceptance = Inacceptable;
            break;

        case PasswordStrength::Long:
            m_text = tr("Long");
            m_hint = tr("Password must be no longer than %1 characters.")
                .arg(PasswordLimitations::kMaximumLengthForCamera);
            m_acceptance = Inacceptable;
            break;

        case PasswordStrength::Conseq:
            m_text = tr("Weak");
            m_hint = tr("Password should not contain %1 or more consecutive characters together.")
                .arg(PasswordLimitations::kConsecutiveCharactersLimit);
            m_acceptance = Inacceptable;
            break;

        case PasswordStrength::Repeat:
            m_text = tr("Weak");
            m_hint = tr("Password should not contain %1 or more repeating characters.")
                .arg(PasswordLimitations::kRepeatingCharactersLimit);
            m_acceptance = Inacceptable;
            break;

        case PasswordStrength::Common:
            m_text = tr("Common");
            m_hint = tr("This password is in list of the most popular passwords.");
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

QnPasswordInformation::QnPasswordInformation(const QString& password, AnalyzeFunction passwordAnalyzer)
    : QnPasswordInformation(passwordAnalyzer(password))
{
}

bool QnPasswordInformation::operator == (const QnPasswordInformation& other) const
{
    return m_text == other.m_text && m_hint == other.m_hint && m_acceptance == other.m_acceptance;
}

bool QnPasswordInformation::operator != (const QnPasswordInformation& other) const
{
    return !(*this == other);
}

const QString& QnPasswordInformation::text() const
{
    return m_text;
}

const QString& QnPasswordInformation::hint() const
{
    return m_hint;
}

QnPasswordInformation::Acceptance QnPasswordInformation::acceptance() const
{
    return m_acceptance;
}
