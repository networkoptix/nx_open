#pragma once

#include <functional>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>

#include <nx/utils/password_analyzer.h>

class QnPasswordInformation
{
public:
    enum Acceptance
    {
        Inacceptable = -1,
        Acceptable,
        Good
    };

    QnPasswordInformation();
    QnPasswordInformation(const QString& text, const QString& hint, Acceptance acceptance);

    QnPasswordInformation(nx::utils::PasswordStrength strength);

    typedef std::function<nx::utils::PasswordStrength(const QString&)> AnalyzeFunction;
    QnPasswordInformation(const QString& password, AnalyzeFunction passwordAnalyzer);

    bool operator == (const QnPasswordInformation& other) const;
    bool operator != (const QnPasswordInformation& other) const;

    const QString& text() const;
    const QString& hint() const;
    Acceptance acceptance() const;

private:
    QString m_text;
    QString m_hint;
    Acceptance m_acceptance;

    Q_DECLARE_TR_FUNCTIONS(PasswordInformation)
};
