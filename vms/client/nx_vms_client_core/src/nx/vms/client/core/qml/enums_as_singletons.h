// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <vector>

#include <QtCore/QMetaEnum>
#include <QtCore/QString>

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::client::core {

/**
 * Registers specified enumeration type as a QML singleton with the name `qmlName` in the library
 * imported from `uri` having the version number composed from `versionMajor` and `versionMinor`.
 * The singleton is created as an immutable JS object with properties matching the
 * enumeration's constants. The enumeration type must be instrumented either within Qt meta system
 * via Q_ENUM, Q_ENUM_NS, Q_FLAG or Q_FLAG_NS, or with NxReflect tools via NX_REFLECTION_ENUM or
 * NX_REFLECTION_ENUM_CLASS (or NX_REFLECTION_ENUM_IN_CLASS or NX_REFLECTION_ENUM_CLASS_IN_CLASS,
 * but using this function for class-scope enums is not recommended as it registers the singleton
 * at library level, losing the scope).
 *
 * This function is introduced to allow access from QML to enumeration constants not accessible
 * using standard QML methods: lowercase non-class enum constants (e.g. of legacy enums kept intact
 * for API backwards compatibility) or constants of enums without associated Qt meta information
 * (for example, namespace-level enums from different header files when it's not desirable to
 * put them all into one super-header with the namespace metaobject - like "qnamespace.h").
 *
 * `qmlName` must start with a capital letter.
 * Names of enumeration constants have no constraints other than C++ language requirements.
 */
template<typename Enum>
int nxRegisterQmlEnumType(
    const char* uri,
    int versionMajor,
    int versionMinor,
    const char* qmlName);

//-------------------------------------------------------------------------------------------------
// Implementation.

namespace detail {

using EnumItem = std::pair<QString, int>;
using EnumItemList = std::vector<EnumItem>;

template<typename Enum>
EnumItemList enumItemList()
{
    static_assert(nx::reflect::IsInstrumentedEnumV<Enum> || QtPrivate::IsQEnumHelper<Enum>::Value,
        "Enumeration is neither instrumented with nx_reflect nor with the Qt meta system.");

    if constexpr (nx::reflect::IsInstrumentedEnumV<Enum>)
    {
        return nx::reflect::enumeration::visitAllItems<Enum>(
            [](auto&&... item)
            {
                return detail::EnumItemList{{detail::EnumItem{
                    QLatin1StringView{item.name.data(), item.name.length()}, (int) item.value}...}};
            });
    }

    if constexpr (QtPrivate::IsQEnumHelper<Enum>::Value)
    {
        const auto metaEnum = QMetaEnum::fromType<Enum>();
        const int count = metaEnum.keyCount();

        EnumItemList result;
        result.reserve(count);

        for (int i = 0; i < count; ++i)
            result.emplace_back(metaEnum.key(i), metaEnum.value(i));

        return result;
    }

    return {};
}

NX_VMS_CLIENT_CORE_API int nxRegisterQmlEnumType(
    const char* uri,
    int versionMajor,
    int versionMinor,
    const char* qmlName,
    EnumItemList&& items);

} // namespace detail

template<typename Enum>
int nxRegisterQmlEnumType(
    const char* uri,
    int versionMajor,
    int versionMinor,
    const char* qmlName)
{
    return detail::nxRegisterQmlEnumType(uri, versionMajor, versionMinor, qmlName,
        detail::enumItemList<Enum>());
}

} // namespace nx::vms::client::core
