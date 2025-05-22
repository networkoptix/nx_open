// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "enums_as_singletons.h"

#include <cctype>

#include <QtQml/QtQml>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::core {
namespace detail {

// TODO: #vkutin Support scoped enumerations with composite names separated with "." if such a need
// arises. Currently the complexity of a generic solution doesn't seem worth the effort:
// - do we want to allow adding an enum to a scope singleton that is already created in an engine?
// - do we want to support multiple engines?

int nxRegisterQmlEnumType(
    const char* uri,
    int versionMajor,
    int versionMinor,
    const char* qmlName,
    EnumItemList&& items)
{
    if (!NX_ASSERT(qmlName && *qmlName && std::isupper((unsigned char) qmlName[0]),
        "Invalid enumeration name \"%1\", must start with an upper-case letter.",
        QString{qmlName}))
    {
        return -1;
    }

    const int typeId = qmlRegisterSingletonType(uri, versionMajor, versionMinor, qmlName,
        [items = std::move(items)](QQmlEngine* /*qmlEngine*/, QJSEngine* jsEngine) -> QJSValue
        {
            auto object = jsEngine->newObject();
            for (const auto& item: items)
                object.setProperty(item.first, item.second);
            const auto freezer = jsEngine->evaluate("(obj => Object.freeze(obj))");
            freezer.call({object}); //< Make object immutable.
            return object;
        });

    NX_ASSERT(typeId >= 0, "Registration of enumeration \"%1\" failed", QString{qmlName});
    return typeId;
}

} // namespace detail
} // namespace nx::vms::client::core
