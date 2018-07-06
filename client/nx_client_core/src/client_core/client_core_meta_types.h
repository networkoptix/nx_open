#pragma once

#include <QtCore/QMetaType>

typedef QSet<QString> QnStringSet;
Q_DECLARE_METATYPE(QnStringSet)

namespace nx {
namespace client {
namespace core {

void initializeMetaTypes();

} // namespace core
} // namespace client
} // namespace nx
