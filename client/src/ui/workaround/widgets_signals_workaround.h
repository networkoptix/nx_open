#ifndef QN_WIDGETS_SIGNALS_WORKAROUND
#define QN_WIDGETS_SIGNALS_WORKAROUND

#define QnComboboxCurrentIndexChanged static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged)
#define QnComboboxActivated static_cast<void (QComboBox::*)(int)>(&QComboBox::activated)

#define QnSpinboxIntValueChanged static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged)

#define QnButtonGroupIdToggled static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled)

#endif //QN_WIDGETS_SIGNALS_WORKAROUND
