#include "device_settings_tab.h"
#include "widgets.h"
#include "device_settings_dlg.h"
#include "settings.h"

CLDeviceSettingsTab::CLDeviceSettingsTab(CLAbstractDeviceSettingsDlg* dlg, QObject* handler, QnResourcePtr dev, QString group)
    : QWidget(dlg),
      mDlg(dlg),
      mHandler(handler),
      mDevice(dev),
      mGroup(group)
{
    setAutoFillBackground(true);
    {
        QPalette pal = palette();
        pal.setColor(backgroundRole(), Qt::black);
        //setPalette(pal);
    }

    //QVBoxLayout *mainLayout = new QVBoxLayout;

    QList<QString> sub_groups = mDevice->getResourceParamList().subGroupList(mGroup);

    int size = 175;

    int x = 0;
    for (int i = 0 ; i < sub_groups.count(); ++i)
    {
        QString sub_group = sub_groups.at(i);

        QWidget* parent = this;

        if (!sub_group.isEmpty())
        {
            QGroupBox *subgroupBox = new QGroupBox(this);
            subgroupBox->setObjectName(group + sub_group);
            subgroupBox->setTitle(sub_group);
            subgroupBox->setFont(settings_font);
            subgroupBox->setFixedSize(size, 420);
            subgroupBox->move(10+x, 10);

            mDlg->putGroup(subgroupBox);

            parent = subgroupBox;

            x += size + 15;
        }

        int y = 0;
        foreach (const QnParam &param, mDevice->getResourceParamList().paramList(group, sub_group).list())
        {
            if (!param.isUiParam())
                continue;

            CLAbstractSettingsWidget* awidget;
            switch (param.type())
            {
            case QnParamType::OnOff:
                awidget = new SettingsOnOffWidget(handler, dev, group, sub_group, param.name());
                break;

            case QnParamType::MinMaxStep:
                awidget = new SettingsMinMaxStepWidget(handler, dev, group, sub_group, param.name());
                break;

            case QnParamType::Enumeration:
                awidget = new SettingsEnumerationWidget(handler, dev, group, sub_group, param.name());
                break;

            case QnParamType::Button:
                awidget = new SettingsButtonWidget(handler, dev, group, sub_group, param.name());
                break;

            default:
                awidget = 0;
                break;
            }
            if (!awidget)
                continue;

            QWidget *widget = awidget->widget();
            widget->setParent(parent);
            widget->setFont(settings_font);

            if (!param.description().isEmpty())
                widget->setToolTip(param.description());

            widget->move(10, 20 + y);
            y += 80;

            mDlg->putWidget(awidget);
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
    return mGroup;
}
