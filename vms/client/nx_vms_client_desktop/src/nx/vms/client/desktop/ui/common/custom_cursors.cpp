// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_cursors.h"

#include <memory>

#include <QtQml/QtQml>

#include <nx/vms/client/core/qml/qml_ownership.h>
#include <nx/vms/client/core/skin/skin.h>

template<> nx::vms::client::desktop::CustomCursors*
    Singleton<nx::vms::client::desktop::CustomCursors>::s_instance = nullptr;

namespace {

nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kCursorCloseTheme = {
    {QnIcon::Normal, {.primary = "dark1", .secondary = "light1"}}};
nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kCursorMoveTheme = {
    {QnIcon::Normal, {.primary = "light1", .secondary = "dark1"}}};

NX_DECLARE_COLORIZED_ICON(kCrossIcon, "32x32/Solid/cursor_cross.svg", kCursorCloseTheme)
NX_DECLARE_COLORIZED_ICON(kMoveIcon, "32x32/Solid/cursor_move.svg", kCursorMoveTheme)

} // namespace

namespace nx::vms::client::desktop {

struct CustomCursors::Private
{
    QCursor sizeAllCursor;
    QCursor crossCursor;
};

const QCursor& CustomCursors::sizeAll = CustomCursors::staticData().sizeAllCursor;
const QCursor& CustomCursors::cross = CustomCursors::staticData().crossCursor;

CustomCursors::Private& CustomCursors::staticData()
{
    static Private data;
    return data;
}

CustomCursors::CustomCursors(core::Skin* skin)
{
    staticData().sizeAllCursor = QCursor{skin->icon(kMoveIcon).pixmap(32, 32)};
    staticData().crossCursor = QCursor{skin->icon(kCrossIcon).pixmap(32, 32)};
}

void CustomCursors::registerQmlType()
{
    qmlRegisterSingletonType<CustomCursors>("nx.vms.client.desktop", 1, 0, "CustomCursors",
        [](QQmlEngine* /*qmlEngine*/, QJSEngine* /*jsEngine*/) -> QObject*
        {
            return core::withCppOwnership(instance());
        });
}

} // namespace nx::vms::client::desktop
