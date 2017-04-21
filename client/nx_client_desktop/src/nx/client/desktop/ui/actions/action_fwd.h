#pragma once

#include <QtCore/QPointer>

class QAction;
class QActionGroup;
class QMenu;

class QnAction;
class QnActionParameters;
class QnResourceCriterion;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

class Manager;
using ManagerPtr = QPointer<Manager>;

class Factory;
using FactoryPtr = QPointer<Factory>;

class TextFactory;
using TextFactoryPtr = QPointer<TextFactory>;

class Condition;
using ConditionPtr = QPointer<Condition>;

class TargetProvider;

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx