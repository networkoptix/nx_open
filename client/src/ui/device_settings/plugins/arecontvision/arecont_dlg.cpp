#include "arecont_dlg.h"

#include <QtGui/QMessageBox>

#include "../../widgets.h"
#include "settings.h"

AreconVisionDlgManufacture::AreconVisionDlgManufacture()
{
    mPossibleNames.push_back(QLatin1String("1300"));
    mPossibleNames.push_back(QLatin1String("2100"));
    mPossibleNames.push_back(QLatin1String("3100"));
    mPossibleNames.push_back(QLatin1String("5100"));

    mPossibleNames.push_back(QLatin1String("1310"));
    mPossibleNames.push_back(QLatin1String("2110"));
    mPossibleNames.push_back(QLatin1String("3110"));
    mPossibleNames.push_back(QLatin1String("5110"));

    mPossibleNames.push_back(QLatin1String("1305"));
    mPossibleNames.push_back(QLatin1String("2105"));
    mPossibleNames.push_back(QLatin1String("3105"));
    mPossibleNames.push_back(QLatin1String("5105"));
    mPossibleNames.push_back(QLatin1String("10005"));

    mPossibleNames.push_back(QLatin1String("1355"));
    mPossibleNames.push_back(QLatin1String("2155"));
    mPossibleNames.push_back(QLatin1String("3155"));
    mPossibleNames.push_back(QLatin1String("5155"));

    mPossibleNames.push_back(QLatin1String("1315"));
    mPossibleNames.push_back(QLatin1String("2115"));
    mPossibleNames.push_back(QLatin1String("3115"));
    mPossibleNames.push_back(QLatin1String("5115"));

    mPossibleNames.push_back(QLatin1String("1325"));
    mPossibleNames.push_back(QLatin1String("2125"));
    mPossibleNames.push_back(QLatin1String("3125"));
    mPossibleNames.push_back(QLatin1String("5125"));

    mPossibleNames.push_back(QLatin1String("2805"));
    mPossibleNames.push_back(QLatin1String("2815"));
    mPossibleNames.push_back(QLatin1String("2825"));

    mPossibleNames.push_back(QLatin1String("8360"));
    mPossibleNames.push_back(QLatin1String("8180"));

    mPossibleNames.push_back(QLatin1String("8365"));
    mPossibleNames.push_back(QLatin1String("8185"));

    mPossibleNames.push_back(QLatin1String("3130"));
    mPossibleNames.push_back(QLatin1String("3135"));
    mPossibleNames.push_back(QLatin1String("20185"));
    mPossibleNames.push_back(QLatin1String("20365"));

    QList<QString> copy = mPossibleNames;

    foreach (const QString &str, copy)
    {
        QString mini = str + QLatin1String("M");
        QString dn = str + QLatin1String("DN");
        QString ai = str + QLatin1String("AI");
        QString ir = str + QLatin1String("IR");

        mPossibleNames.push_back(mini);
        mPossibleNames.push_back(dn);
        mPossibleNames.push_back(ai);
        mPossibleNames.push_back(ir);
    }

}

CLAbstractDeviceSettingsDlg *AreconVisionDlgManufacture::createDlg(QnResourcePtr resource)
{
    return new AVSettingsDlg(resource);
}

bool AreconVisionDlgManufacture::canProduceDlg(QnResourcePtr resource) const
{
    return mPossibleNames.contains(resource->getName());
}

//=======================================================================================
AVSettingsDlg::AVSettingsDlg(QnResourcePtr resource, QWidget *parent)
    : CLAbstractDeviceSettingsDlg(resource, parent)
{
    initTabsOrder();
    initImageQuality();
    initExposure();
    initAI();
    initDN();
    initMD();
    initAdmin();

    correctWgtsState();
}

void AVSettingsDlg::initTabsOrder()
{
    CLDeviceSettingsTab* tab = 0;

    if (tab = tabByName(tr("Image quality")))
        addTab(tab);

    if (tab = tabByName(tr("Exposure")))
        addTab(tab);

    if (tab = tabByName(tr("AutoIris")))
        addTab(tab);

    if (tab = tabByName(tr("Day/Night")))
        addTab(tab);

    if (tab=tabByName(tr("Binning")))
        addTab(tab);

    if (tab = tabByName(tr("Motion detection")))
        addTab(tab);

    if (tab = tabByName(tr("Network")))
        addTab(tab);

    if (tab = tabByName(tr("Administration/Info")))
        addTab(tab);
}

void AVSettingsDlg::initImageQuality()
{
    if (QGroupBox *group = getGroupByName(QLatin1String("Quality")))
    {
        group->move(10,10);
        group->setFixedWidth(350);
        group->setFixedHeight(205);

        if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Quality")))
        {
            if (getWidgetByName(QLatin1String("Codec")) != 0)
            {
                wgt->widget()->move(10,35);
            }
            else
            {
                wgt->widget()->move(10,73);
                wgt->widget()->setFixedWidth(330);
            }
        }

        if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Codec")))
            wgt->widget()->move(200,25);

        if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Bitrate")))
        {
            wgt->widget()->move(10,125);
            wgt->widget()->setFixedWidth(330);
        }
    }

    if (QGroupBox *group = getGroupByName(QLatin1String("Color")))
    {
        group->move(385,10);
        group->setFixedWidth(180);
        group->setFixedHeight(205);

        if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Red")))
            wgt->widget()->move(10,30);

        if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Blue")))
            wgt->widget()->move(10,120);
    }

    if (QGroupBox *group = getGroupByName(QLatin1String("Adjustment")))
    {
        group->move(10,240);
        group->setFixedWidth(555);
        group->setFixedHeight(110);

        if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Brightness")))
            wgt->widget()->move(10,30);

        if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Sharpness")))
            wgt->widget()->move(200,30);

        if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Saturation")))
            wgt->widget()->move(380,30);
    }

    if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Equalize Brightness")))
    {
        wgt->widget()->move(135,360);

        if (wgt = getWidgetByName(QLatin1String("Equalize Color")))
            wgt->widget()->move(340,360);

        if (wgt = getWidgetByName(QLatin1String("Rotate 180")))
            wgt->widget()->hide(); // for now
    }
    else
    {
        if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Rotate 180")))
            wgt->widget()->move(255,360);
    }
    //
}

void AVSettingsDlg::initExposure()
{
    if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Illumination")))
        wgt->widget()->move(420,60);

    if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Lighting")))
        wgt->widget()->move(420,210);

    if (QGroupBox *group = getGroupByName(QLatin1String("Low Light Mode")))
    {
        group->move(10,60);
        group->setFixedWidth(380);
        group->setFixedHeight(240);

        if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Light Mode")))
            wgt->widget()->move(30,30);

        if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Short Exposure")))
            wgt->widget()->move(200,75);

        if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Auto exposure On/Off")))
            wgt->widget()->move(30,200);
    }
}

void AVSettingsDlg::initAI()
{
    if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("AutoIris enable")))
        wgt->widget()->move(200,180);
}

void AVSettingsDlg::initDN()
{
    if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Day/Night Mode")))
        wgt->widget()->move(215,130);
}

void AVSettingsDlg::initMD()
{
    //Enable motion detection
    //Zone size
    //Threshold
    //Sensitivity
    //Md detail

    if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Enable motion detection")))
        wgt->widget()->move(175,30);

    if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Zone size")))
        wgt->widget()->move(80,80);

    if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Threshold")))
        wgt->widget()->move(320,80);

    if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Sensitivity")))
        wgt->widget()->move(80,210);

    if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Md detail")))
        wgt->widget()->move(320,210);
}

void AVSettingsDlg::initAdmin()
{
    if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Factory defaults")))
        wgt->widget()->move(230,150);
}

//=========================================================================
void AVSettingsDlg::setParam(const QString& name, const QVariant& val)
{
    CLAbstractDeviceSettingsDlg::setParam(name, val);
    correctWgtsState();
}

void AVSettingsDlg::onSuggestions()
{
    QString text = tr("To reduce the bandwidth try to set Light Mode on Exposure tab to HightSpeed and set Short Exposure to 30ms.");
    QMessageBox mbox(QMessageBox::Information, tr("Suggestion"), text, QMessageBox::Ok, this, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    //mbox.setWindowOpacity(global_dlg_opacity);
    mbox.exec();
}

void AVSettingsDlg::onClose()
{
    CLAbstractDeviceSettingsDlg::setParam(QLatin1String("Save to flash"), "");
    CLAbstractDeviceSettingsDlg::close();
}

void AVSettingsDlg::correctWgtsState()
{
    if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Light Mode")))
    {
        QString val = wgt->param().value().toString();
        if (wgt = getWidgetByName(QLatin1String("Short Exposure")))
            wgt->widget()->setVisible(val == QLatin1String("highspeed"));
    }

    if (CLAbstractSettingsWidget *wgt = getWidgetByName(QLatin1String("Codec")))
    {
        QString val = wgt->param().value().toString();
        if (wgt = getWidgetByName(QLatin1String("Bitrate")))
            wgt->widget()->setVisible(val == QLatin1String("H.264"));
    }
}
