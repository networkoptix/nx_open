// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/reflect/type_utils.h>

namespace nx::reflect::test {

static_assert(!nx::reflect::IsContainerV<QString>, "!IsContainerV, QString");
static_assert(!nx::reflect::IsContainerV<QByteArray>, "!IsContainerV, QByteArray");

} // namespace nx::reflect::test
