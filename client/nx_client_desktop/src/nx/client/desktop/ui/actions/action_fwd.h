#pragma once

#include <QtCore/QPointer>

class QnAction;
class QnActionManager;
class QnActionParameters;
class QnResourceCriterion;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

class Factory;
using FactoryPtr = QPointer<Factory>;

class TextFactory;
using TextFactoryPtr = QPointer<TextFactory>;

class Condition;
using ConditionPtr = QPointer<Condition>;

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx