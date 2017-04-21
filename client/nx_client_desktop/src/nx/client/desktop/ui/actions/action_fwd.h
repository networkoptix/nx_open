#pragma once

#include <QtCore/QPointer>

class QnAction;
class QnActionFactory;
class QnActionTextFactory;
class QnResourceCriterion;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

class Condition;
using ConditionPtr = QPointer<Condition>;

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx