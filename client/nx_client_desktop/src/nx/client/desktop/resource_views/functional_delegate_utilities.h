#pragma once

#include <QtCore/QAbstractListModel>
#include <core/resource/resource_fwd.h>

class QnCustomizableItemDelegate;
class QnWorkbenchContext;

namespace nx {
namespace client {
namespace desktop {

// Custom accessor to get a string with resource version.
QVariant resourceVersionAccessor(QnResourcePtr ptr, int role);

// Creates column-specific delegate, that applies color to resource version.
QnCustomizableItemDelegate * makeVersionStatusDelegate(QnWorkbenchContext* context, QObject* parent);

} // namespace desktop
} // namespace client
} // namespace nx
