// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "translation.h"

#include <nx/utils/log/format.h>
#include <nx/utils/log/to_string.h>

namespace nx::i18n {

QString Translation::toString() const
{
    return NX_FMT("%1 %2", localeCode, containerString(filePaths));
}

} // namespace nx::i18n
