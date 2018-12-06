#pragma once

#include <QtCore/QPointer>

class QAction;
class QActionGroup;
class QMenu;

namespace nx::vms::client::desktop {
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
} // namespace nx::vms::client::desktop
