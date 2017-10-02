#pragma once

#include <QtCore/QVector>
#include <QtCore/QMetaType>
#include <QtCore/QMargins>

#include <QtGui/QColor>

#include <common/common_meta_types.h>
#include <client_core/client_core_meta_types.h>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

/**
 * Convenience class for uniform initialization of metatypes in client module.
 *
 * Also initializes metatypes from common and client_core module.
 */
class QnClientMetaTypes {
public:
    static void initialize();
};

Q_DECLARE_METATYPE(Qt::KeyboardModifiers)
Q_DECLARE_METATYPE(QVector<QnUuid>)
Q_DECLARE_METATYPE(QVector<QColor>)
Q_DECLARE_METATYPE(QMargins)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((Qt::Alignment)(Qt::DateFormat), (lexical))
