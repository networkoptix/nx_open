#include "device_settings_tab.h"

#include <QtGui/QItemEditorFactory>

#include "ui/style/globals.h"

#include "device_settings_dlg.h"
#include "utils/settings.h"
#include "widgets.h"

CLDeviceSettingsTab::CLDeviceSettingsTab(CLAbstractDeviceSettingsDlg *dialog, const QnParamList &paramList, const QString &group)
    : QWidget(dialog)
{
    //QVBoxLayout *mainLayout = new QVBoxLayout;

    int size = 175;

    int x = 0;
    foreach (const QString &sub_group, paramList.subGroupList(group)) {
        QWidget *parent = this;

        if (!sub_group.isEmpty()) {
            QGroupBox *subgroupBox = new QGroupBox(this);
            subgroupBox->setObjectName(group + sub_group);
            subgroupBox->setTitle(sub_group);
            subgroupBox->setFont(qnGlobals->settingsFont());
            subgroupBox->setFixedSize(size, 420);
            subgroupBox->move(10+x, 10);

            dialog->putGroup(sub_group, subgroupBox);

            parent = subgroupBox;

            x += size + 15;
        }

        int y = 0;

        QScopedPointer<SettingsEditorFactory> editorFactory(new SettingsEditorFactory);
        foreach (const QnParam &param, paramList.paramList(group, sub_group).list()) {
            if (!param.isUiParam())
                continue;

            QWidget *widget = editorFactory->createEditor(param, parent);
            if (!widget)
                continue;

            widget->setParent(parent); // ### remove
            widget->setFont(qnGlobals->settingsFont());
            widget->setEnabled(!param.isReadOnly());
            widget->setToolTip(param.description());

            dialog->registerWidget(widget, param);

            widget->move(10, 20 + y);
            y += 80;
        }
        //mainLayout->addWidget(subgroupBox);
    }

    //setLayout(mainLayout);
}
