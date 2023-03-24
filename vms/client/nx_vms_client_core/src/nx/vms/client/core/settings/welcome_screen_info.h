// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaObject>
#include <QtCore/QMetaType>

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::client::core::welcome_screen {

Q_NAMESPACE_EXPORT(NX_VMS_CLIENT_CORE_API)

NX_REFLECTION_ENUM_CLASS(TileVisibilityScope,
    HiddenTileVisibilityScope = -1,
    DefaultTileVisibilityScope = 0,
    FavoriteTileVisibilityScope = 1
)

/**
  * AllSystems - Show systems which visibility scope is Default or Favorite.
  * Favorites - Show systems which visibility scope is Favorite.
  * Hidden - Show systems which visibility scope is Hidden.
  */
NX_REFLECTION_ENUM_CLASS(TileScopeFilter,
    HiddenTileScopeFilter,
    AllSystemsTileScopeFilter,
    FavoritesTileScopeFilter
)

Q_ENUM_NS(TileScopeFilter)
Q_ENUM_NS(TileVisibilityScope)

} // namespace nx::vms::client::core::welcome_screen
