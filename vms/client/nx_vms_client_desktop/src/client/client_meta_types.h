// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QVector>
#include <QtCore/QMetaType>
#include <QtCore/QMargins>
#include <QtGui/QColor>
#include <QtGui/QValidator>

#include <common/common_meta_types.h>
#include <client_core/client_core_meta_types.h>

#include <nx/utils/uuid.h>

/**
 * Convenience class for uniform initialization of metatypes in client module.
 *
 * Also initializes metatypes from common and client_core module.
 */
class NX_VMS_CLIENT_DESKTOP_API QnClientMetaTypes
{
public:
    static void initialize();

private:
    static void registerQmlTypes();
};

Q_DECLARE_METATYPE(Qt::KeyboardModifiers)
Q_DECLARE_METATYPE(QVector<QnUuid>)
Q_DECLARE_METATYPE(QVector<QColor>)
Q_DECLARE_METATYPE(QMargins)
Q_DECLARE_METATYPE(QValidator::State) //< For Qn::ValidationStateRole QVariant conversion.
