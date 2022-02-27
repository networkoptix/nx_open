// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#define QnComboboxCurrentIndexChanged   static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged)
#define QnComboboxActivated             static_cast<void (QComboBox::*)(int)>(&QComboBox::activated)
#define QnSpinboxIntValueChanged        static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged)
#define QnSpinboxDoubleValueChanged     static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged)
#define QnButtonGroupIdToggled          static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled)
#define QnCompleterActivated            static_cast<void (QCompleter::*)(const QString&)>(&QCompleter::activated)
#define QnButtonGroupIntButtonClicked   static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked)
