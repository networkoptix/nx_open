#include "translatable_string.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace api {

QString TranslatableString::text(const QString& locale) const
{
    return localization.value(locale, value);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TranslatableString, (json), TranslatableString_Fields,
    (brief, true))

} // namespace api
} // namespace nx
