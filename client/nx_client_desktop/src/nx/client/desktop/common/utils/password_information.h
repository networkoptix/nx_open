#pragma once

#include <functional>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>

#include <nx/utils/password_analyzer.h>

namespace nx {
namespace client {
namespace desktop {

class PasswordInformation
{
public:
    PasswordInformation() = default;
    PasswordInformation(const QString& text, const QString& hint, utils::PasswordAcceptance acceptance);

    explicit PasswordInformation(nx::utils::PasswordStrength strength);

    typedef std::function<nx::utils::PasswordStrength(const QString&)> AnalyzeFunction;
    PasswordInformation(const QString& password, AnalyzeFunction passwordAnalyzer);

    bool operator==(const PasswordInformation& other) const;
    bool operator!=(const PasswordInformation& other) const;

    QString text() const;
    QString hint() const;
    utils::PasswordAcceptance acceptance() const;

private:
    QString m_text;
    QString m_hint;
    utils::PasswordAcceptance m_acceptance = utils::PasswordAcceptance::Unacceptable;

    Q_DECLARE_TR_FUNCTIONS(PasswordInformation)
};

} // namespace desktop
} // namespace client
} // namespace nx
