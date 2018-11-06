#pragma once

#include <functional>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>

#include <nx/utils/password_analyzer.h>

namespace nx::vms::client::desktop {

class PasswordInformation
{
public:
    PasswordInformation() = default;
    PasswordInformation(
        const QString& text,
        const QString& hint,
        nx::utils::PasswordAcceptance acceptance);

    explicit PasswordInformation(nx::utils::PasswordStrength strength);

    typedef std::function<nx::utils::PasswordStrength(const QString&)> AnalyzeFunction;
    PasswordInformation(const QString& password, AnalyzeFunction passwordAnalyzer);

    bool operator==(const PasswordInformation& other) const;
    bool operator!=(const PasswordInformation& other) const;

    QString text() const;
    QString hint() const;
    nx::utils::PasswordAcceptance acceptance() const;

private:
    QString m_text;
    QString m_hint;
    nx::utils::PasswordAcceptance m_acceptance = nx::utils::PasswordAcceptance::Unacceptable;

    Q_DECLARE_TR_FUNCTIONS(PasswordInformation)
};

} // namespace nx::vms::client::desktop
