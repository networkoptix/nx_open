#ifndef QN_QT5_COMBOBOX_WORKAROUND
#define QN_QT5_COMBOBOX_WORKAROUND

#define QnComboboxCurrentIndexChanged static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged)
#define QnComboboxActivated static_cast<void (QComboBox::*)(int)>(&QComboBox::activated)

#endif //QN_QT5_COMBOBOX_WORKAROUND