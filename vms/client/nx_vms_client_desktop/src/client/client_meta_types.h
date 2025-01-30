// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMargins>
#include <QtCore/QMetaType>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtGui/QValidator>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

/**
 * Also initializes metatypes from common and client_core module.
 */
NX_VMS_CLIENT_DESKTOP_API void initializeMetaTypes();

/**
 * Also registers QML types from client_core module.
 */
NX_VMS_CLIENT_DESKTOP_API void registerQmlTypes();

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(Qt::KeyboardModifiers)
Q_DECLARE_METATYPE(QVector<nx::Uuid>)
Q_DECLARE_METATYPE(QVector<QColor>)
Q_DECLARE_METATYPE(QMargins)
Q_DECLARE_METATYPE(QValidator::State) //< For Qn::ValidationStateRole QVariant conversion.
