#pragma once

#include <QtCore/QString>

namespace nx::vms::server::interactive_settings {

struct Issue
{
    enum class Code
    {
        ioError,
        itemNameIsNotUnique,
        parseError,
        valueConverted,
        cannotConvertValue,
    };

    enum class Type
    {
        warning,
        error,
    };

    Type type;
    Code code;
    QString message;

    Issue(Type type, Code code, const QString& message = QString()):
        type(type), code(code), message(message)
    {
    }
};

QString toString(const Issue::Code& code);

} // namespace nx::vms::server::interactive_settings
