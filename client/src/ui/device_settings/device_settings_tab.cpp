#include "device_settings_tab.h"

#include <QtGui/QItemEditorFactory>

#include "widgets.h"
#include "device_settings_dlg.h"
#include "settings.h"

CLDeviceSettingsTab::CLDeviceSettingsTab(CLAbstractDeviceSettingsDlg *dialog, QnResourcePtr resource, const QString &group)
    : QWidget(dialog),
      m_dialog(dialog),
      m_resource(resource),
      m_group(group)
{
    //QVBoxLayout *mainLayout = new QVBoxLayout;

    int size = 175;

    int x = 0;
    foreach (const QString &sub_group, m_resource->getResourceParamList().subGroupList(m_group))
    {
        QWidget *parent = this;

        if (!sub_group.isEmpty())
        {
            QGroupBox *subgroupBox = new QGroupBox(this);
            subgroupBox->setObjectName(group + sub_group);
            subgroupBox->setTitle(sub_group);
            subgroupBox->setFont(settings_font);
            subgroupBox->setFixedSize(size, 420);
            subgroupBox->move(10+x, 10);

            m_dialog->putGroup(sub_group, subgroupBox);

            parent = subgroupBox;

            x += size + 15;
        }

        int y = 0;
        foreach (const QnParam &param, m_resource->getResourceParamList().paramList(group, sub_group).list())
        {
            if (!param.isUiParam())
                continue;

            CLAbstractSettingsWidget *awidget = 0;
            switch (param.type())
            {
            case QnParamType::OnOff:
                awidget = new SettingsOnOffWidget(m_resource, param);
                break;

            case QnParamType::MinMaxStep:
                awidget = new SettingsMinMaxStepWidget(m_resource, param);
                break;

            case QnParamType::Enumeration:
                awidget = new SettingsEnumerationWidget(m_resource, param);
                break;

            case QnParamType::Button:
                awidget = new SettingsButtonWidget(m_resource, param);
                break;

            default:
                //awidget = QItemEditorFactory::defaultFactory()->createEditor(param.defaultValue().userType(), parent); // ###
                break;
            }
            if (awidget) {
                connect(awidget, SIGNAL(setParam(QString,QVariant)), m_dialog, SLOT(setParam(QString,QVariant)));

                QWidget *widget = awidget->widget();
                widget->setParent(parent);
                widget->setFont(settings_font);

                if (!param.description().isEmpty())
                    widget->setToolTip(param.description());

                widget->move(10, 20 + y);
                y += 80;

                m_dialog->putWidget(awidget);
            }
        }
        //mainLayout->addWidget(subgroupBox);
    }

    //setLayout(mainLayout);
}

CLDeviceSettingsTab::~CLDeviceSettingsTab()
{
}

QString CLDeviceSettingsTab::name() const
{
    return m_group;
}
