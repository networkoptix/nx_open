// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QtGlobal>
#include <nx/utils/std/optional.h>

class QSpinBox;
class QDoubleSpinBox;

namespace nx::vms::client::desktop {
namespace spin_box_utils {

// Integer value spin boxes.

void autoClearSpecialValue(QSpinBox* spinBox, int startingValue);

void setupSpinBox(QSpinBox* spinBox, bool sameValue, int value);
void setupSpinBox(QSpinBox* spinBox, std::optional<int> value);
void setupSpinBox(QSpinBox* spinBox, bool sameValue, int value, int min, int max);
void setupSpinBox(QSpinBox* spinBox, std::optional<int> value, int min, int max);

std::optional<int> value(QSpinBox* spinBox);

void setSpecialValue(QSpinBox* spinBox);
bool isSpecialValue(QSpinBox* spinBox);

// Floating-point value spin boxes.

void autoClearSpecialValue(QDoubleSpinBox* spinBox, qreal startingValue);

void setupSpinBox(QDoubleSpinBox* spinBox, bool sameValue, qreal value);
void setupSpinBox(QDoubleSpinBox* spinBox, std::optional<qreal> value);
void setupSpinBox(QDoubleSpinBox* spinBox, bool sameValue, qreal value, qreal min, qreal max);
void setupSpinBox(QDoubleSpinBox* spinBox, std::optional<qreal> value, qreal min, qreal max);

std::optional<qreal> value(QDoubleSpinBox* spinBox);

void setSpecialValue(QDoubleSpinBox* spinBox);
bool isSpecialValue(QDoubleSpinBox* spinBox);

} // namespace spin_box_utils
} // namespace nx::vms::client::desktop
