#pragma once

#define QnComboboxCurrentIndexChanged   static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged)
#define QnComboboxActivated             static_cast<void (QComboBox::*)(int)>(&QComboBox::activated)
#define QnSpinboxIntValueChanged        static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged)
#define QnSpinboxDoubleValueChanged     static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged)
#define QnButtonGroupIdToggled          static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled)
#define QnDoubleSpinBoxValueChanged     static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged)
#define QnTimeDurationWidgetValueChanged        static_cast<void (QnTimeDurationWidget::*)(int)>(&QnTimeDurationWidget::valueChanged)
