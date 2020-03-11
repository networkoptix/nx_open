#include "text_field.h"

namespace nx::vms::server::interactive_settings::components {

TextField::TextField(QObject* parent):
    StringValueItem(QStringLiteral("TextField"), parent)
{
}

QString TextField::validationRegex() const
{
    return m_validationRegex;
}

void TextField::setValidationRegex(const QString& regex)
{
    if (m_validationRegex == regex)
        return;

    m_validationRegex = regex;
    emit validationRegexChanged();
}

QString TextField::validationRegexFlags() const
{
    return m_validationRegexFlags;
}

void TextField::setValidationRegexFlags(const QString& flags)
{
    if (m_validationRegexFlags == flags)
        return;

    m_validationRegexFlags = flags;
    emit validationRegexFlagsChanged();
}

QString TextField::validationErrorMessage() const
{
    return m_validationErrorMessage;
}

void TextField::setValidationErrorMessage(const QString& message)
{
    if (m_validationErrorMessage == message)
        return;

    m_validationErrorMessage = message;
    emit validationErrorMessageChanged();
}

QJsonObject TextField::serializeModel() const
{
    auto result = StringValueItem::serializeModel();
    result[QStringLiteral("validationRegex")] = validationRegex();
    result[QStringLiteral("validationRegexFlags")] = validationRegexFlags();
    result[QStringLiteral("validationErrorMessage")] = validationErrorMessage();
    return result;
}

} // namespace nx::vms::server::interactive_settings::components
