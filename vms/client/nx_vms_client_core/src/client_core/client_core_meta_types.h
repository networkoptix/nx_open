#pragma once

#include <QtCore/QMetaType>

typedef QSet<QString> QnStringSet;
Q_DECLARE_METATYPE(QnStringSet)

namespace nx::vms::client::core {

void initializeMetaTypes();

} // namespace nx::vms::client::core
