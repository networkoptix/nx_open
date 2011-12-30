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

    QList<QString> base = mPossibleNames;

    foreach (const QString &str, base)
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

AreconVisionDlgManufacture& AreconVisionDlgManufacture::instance()
{
    static AreconVisionDlgManufacture inst;
    return inst;
}

CLAbstractDeviceSettingsDlg* AreconVisionDlgManufacture::createDlg(QnResourcePtr dev)
{
    return new AVSettingsDlg(dev);
}

bool AreconVisionDlgManufacture::canProduceDlg(QnResourcePtr dev) const
{
    return mPossibleNames.contains(dev->getName());
}

//=======================================================================================
AVSettingsDlg::AVSettingsDlg(QnResourcePtr dev):
CLAbstractDeviceSettingsDlg(dev)
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
    //return;

    CLDeviceSettingsTab* tab = 0;

    if ( (tab = tabByName(tr("Image quality"))) )
        addTabWidget(tab);

    if ( (tab = tabByName(tr("Exposure"))) )
        addTabWidget(tab);

    if ( (tab = tabByName(tr("AutoIris"))) )
        addTabWidget(tab);

    if ( (tab = tabByName(tr("Day/Night"))) )
        addTabWidget(tab);

    if ( (tab=tabByName(tr("Binning"))) )
        addTabWidget(tab);

    if ( (tab = tabByName(tr("Motion detection"))) )
        addTabWidget(tab);

    if ( (tab = tabByName(tr("Network"))) )
        addTabWidget(tab);

    if ( (tab = tabByName(tr("Administration/Info"))) )
        addTabWidget(tab);
}

void AVSettingsDlg::initImageQuality()
{
    QGroupBox* group;
    CLAbstractSettingsWidget* wgt;

    if ( (group = getGroupByName(QLatin1String("Quality"))) )
    {
        group->move(10,10);
        group->setFixedWidth(350);
        group->setFixedHeight(205);

        bool h264 = (getWidgetByName(QLatin1String("Codec")) != 0);

        if ( (wgt = getWidgetByName(QLatin1String("Quality"))) )
        {
            if (h264)
                wgt->toWidget()->move(10,35);
            else
            {
                wgt->toWidget()->move(10,73);
                wgt->toWidget()->setFixedWidth(330);
            }

        }

        if ( (wgt = getWidgetByName(QLatin1String("Codec"))) )
            wgt->toWidget()->move(200,25);

        if ( (wgt = getWidgetByName(QLatin1String("Bitrate"))) )
        {
            wgt->toWidget()->move(10,125);
            wgt->toWidget()->setFixedWidth(330);
        }

    }

    if ( (group = getGroupByName(QLatin1String("Color"))) )
    {
        group->move(385,10);
        group->setFixedWidth(180);
        group->setFixedHeight(205);

        if ( (wgt = getWidgetByName(QLatin1String("Red"))) )
            wgt->toWidget()->move(10,30);

        if ( (wgt = getWidgetByName(QLatin1String("Blue"))) )
            wgt->toWidget()->move(10,120);

    }

    if ( (group = getGroupByName(QLatin1String("Adjustment"))) )
    {
        group->move(10,240);
        group->setFixedWidth(555);
        group->setFixedHeight(110);

        if ( (wgt = getWidgetByName(QLatin1String("Brightness"))) )
            wgt->toWidget()->move(10,30);

        if ( (wgt = getWidgetByName(QLatin1String("Sharpness"))) )
            wgt->toWidget()->move(200,30);

        if ( (wgt = getWidgetByName(QLatin1String("Saturation"))) )
            wgt->toWidget()->move(380,30);

    }

    if ( (wgt = getWidgetByName(QLatin1String("Equalize Brightness"))) )
    {
        wgt->toWidget()->move(135,360);

        if ( (wgt = getWidgetByName(QLatin1String("Equalize Color"))) )
            wgt->toWidget()->move(340,360);

        if ( (wgt = getWidgetByName(QLatin1String("Rotate 180"))) )
            wgt->toWidget()->hide(); // for now

    }
    else
    {
        if ( (wgt = getWidgetByName(QLatin1String("Rotate 180"))) )
            wgt->toWidget()->move(255,360);
    }

    //

}

void AVSettingsDlg::initExposure()
{
    QGroupBox* group;
    CLAbstractSettingsWidget* wgt;

    if ( (wgt = getWidgetByName(QLatin1String("Illumination"))) )
        wgt->toWidget()->move(420,60);

    if ( (wgt = getWidgetByName(QLatin1String("Lighting"))) )
        wgt->toWidget()->move(420,210);

    if ( (group = getGroupByName(QLatin1String("Low Light Mode"))) )
    {
        group->move(10,60);
        group->setFixedWidth(380);
        group->setFixedHeight(240);

        if ( (wgt = getWidgetByName(QLatin1String("Light Mode"))) )
            wgt->toWidget()->move(30,30);

        if ( (wgt = getWidgetByName(QLatin1String("Short Exposure"))) )
            wgt->toWidget()->move(200,75);

        if ( (wgt = getWidgetByName(QLatin1String("Auto exposure On/Off"))) )
            wgt->toWidget()->move(30,200);
    }
}

void AVSettingsDlg::initAI()
{
//    QGroupBox* group;
    CLAbstractSettingsWidget* wgt;

    if ( (wgt = getWidgetByName(QLatin1String("AutoIris enable"))) )
        wgt->toWidget()->move(200,180);
}

void AVSettingsDlg::initDN()
{
//    QGroupBox* group;
    CLAbstractSettingsWidget* wgt;

    if ( (wgt = getWidgetByName(QLatin1String("Day/Night Mode"))) )
        wgt->toWidget()->move(215,130);
}

void AVSettingsDlg::initMD()
{
//    QGroupBox* group;
    CLAbstractSettingsWidget* wgt;

    //Enable motion detection
    //Zone size
    //Threshold
    //Sensitivity
    //Md detail

    if ( (wgt = getWidgetByName(QLatin1String("Enable motion detection"))) )
        wgt->toWidget()->move(175,30);

    if ( (wgt = getWidgetByName(QLatin1String("Zone size"))) )
        wgt->toWidget()->move(80,80);

    if ( (wgt = getWidgetByName(QLatin1String("Threshold"))) )
        wgt->toWidget()->move(320,80);

    if ( (wgt = getWidgetByName(QLatin1String("Sensitivity"))) )
        wgt->toWidget()->move(80,210);

    if ( (wgt = getWidgetByName(QLatin1String("Md detail"))) )
        wgt->toWidget()->move(320,210);
}

void AVSettingsDlg::initAdmin()
{
//    QGroupBox* group;
    CLAbstractSettingsWidget* wgt;

    if ( (wgt = getWidgetByName(QLatin1String("Factory defaults"))) )
        wgt->toWidget()->move(230,150);
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
    CLAbstractSettingsWidget* wgt;
    QString val;

    if (wgt = getWidgetByName(QLatin1String("Light Mode")))
    {
        val = wgt->param().value().toString();
        if (wgt = getWidgetByName(QLatin1String("Short Exposure")))
            wgt->toWidget()->setVisible(val == QLatin1String("highspeed"));
    }

    //=================================================
    if (wgt = getWidgetByName(QLatin1String("Codec")))
    {
        val = wgt->param().value().toString();
        if (wgt = getWidgetByName(QLatin1String("Bitrate")))
            wgt->toWidget()->setVisible(val == QLatin1String("H.264"));
    }
}
