#pragma once

#include <QtCore/QPointer>

class QAction;
class QActionGroup;
class QMenu;

class QnResourceCriterion;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

class Action;

class Parameters;

class Manager;
using ManagerPtr = QPointer<Manager>;

class Factory;
using FactoryPtr = QPointer<Factory>;

class TextFactory;
using TextFactoryPtr = QPointer<TextFactory>;

class Condition;
class ConditionWrapper;

class TargetProvider;

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx