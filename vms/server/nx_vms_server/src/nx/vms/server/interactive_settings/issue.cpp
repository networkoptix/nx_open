#include "issue.h"

namespace nx::vms::server::interactive_settings {

QString toString(const Issue::Code& code)
{
    switch (code)
    {
        case Issue::Code::ioError:
            return "ioError";
        case Issue::Code::itemNameIsNotUnique:
            return "itemNameIsNotUnique";
        case Issue::Code::parseError:
            return "parseError";
        case Issue::Code::valueConverted:
            return "valueConverted";
        case Issue::Code::cannotConvertValue:
            return "cannotConvertValue";
    }
    return {};
}

} // namespace nx::vms::server::interactive_settings
