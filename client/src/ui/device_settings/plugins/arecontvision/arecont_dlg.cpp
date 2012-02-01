#include "arecont_dlg.h"

#include <QtGui/QMessageBox>

AreconVisionDlgManufacture::AreconVisionDlgManufacture()
{
    static const int models[] = {
        1300, 2100, 3100, 5100,
        1310, 2110, 3110, 5110,
        1305, 2105, 3105, 5105, 10005,
        1355, 2155, 3155, 5155,
        1315, 2115, 3115, 5115,
        1325, 2125, 3125, 5125,
        2805, 2815, 2825,
        8360, 8180,
        8365, 8185,
        3130, 3135, 20185, 20365
    };

    for (int i = 0; i < sizeof(models) / sizeof(models[0]); ++i) {
        const QString model = QString::number(models[i]);
        mPossibleNames.append(model);
        mPossibleNames.append(model + QLatin1String("M"));
        mPossibleNames.append(model + QLatin1String("DN"));
        mPossibleNames.append(model + QLatin1String("AI"));
        mPossibleNames.append(model + QLatin1String("IR"));
    }
}

bool AreconVisionDlgManufacture::canProduceDlg(QnResourcePtr resource) const
{
    if (QnResourceTypePtr resType = qnResTypePool->getResourceType(resource->getTypeId())) {
        return resType->getManufacture() == QLatin1String("ArecontVision") &&
               mPossibleNames.contains(resType->getName());
    }

    return false;
}

QDialog *AreconVisionDlgManufacture::createDlg(QnResourcePtr resource, QWidget *parent)
{
    return new AVSettingsDlg(resource, parent);
}


AVSettingsDlg::AVSettingsDlg(QnResourcePtr resource, QWidget *parent)
    : CLAbstractDeviceSettingsDlg(resource, parent)
{
    QPushButton *suggestionsButton = m_buttonBox->addButton(tr("Suggestions..."), QDialogButtonBox::ActionRole);
    suggestionsButton->setFocusPolicy(Qt::NoFocus);
    connect(suggestionsButton, SIGNAL(clicked()), this, SLOT(onSuggestions()));

    initImageQuality();
    initExposure();
    initAI();
    initDN();
    initMD();
    initAdmin();

    connect(resource.data(), SIGNAL(parameterValueChanged(QnParam)), this, SLOT(paramValueChanged(QnParam)));

    correctWgtsState();
}

void AVSettingsDlg::accept()
{
    setParam(QLatin1String("Save to flash"), QString());
    CLAbstractDeviceSettingsDlg::accept();
}

void AVSettingsDlg::buildTabs()
{
    QList<QString> tabsOrder;
    tabsOrder << QString()
              << QLatin1String("Image quality")
              << QLatin1String("Exposure")
              << QLatin1String("AutoIris")
              << QLatin1String("Day/Night")
              << QLatin1String("Binning")
              << QLatin1String("Motion detection")
              << QLatin1String("Administration/Info");

    CLAbstractDeviceSettingsDlg::buildTabs(tabsOrder);
}

void AVSettingsDlg::initImageQuality()
{
    if (QGroupBox *group = groupByName(QLatin1String("Quality"))) {
        group->move(10,10);
        group->setFixedWidth(350);
        group->setFixedHeight(205);

        if (QWidget *wgt = widgetByName(QLatin1String("Quality"))) {
            if (widgetByName(QLatin1String("Codec"))) {
                wgt->move(10,35);
            } else {
                wgt->move(10,73);
                wgt->setFixedWidth(330);
            }
        }

        if (QWidget *wgt = widgetByName(QLatin1String("Codec")))
            wgt->move(200,25);

        if (QWidget *wgt = widgetByName(QLatin1String("Bitrate"))) {
            wgt->move(10,125);
            wgt->setFixedWidth(330);
        }
    }

    if (QGroupBox *group = groupByName(QLatin1String("Color"))) {
        group->move(385,10);
        group->setFixedWidth(180);
        group->setFixedHeight(205);

        if (QWidget *wgt = widgetByName(QLatin1String("Red")))
            wgt->move(10,30);

        if (QWidget *wgt = widgetByName(QLatin1String("Blue")))
            wgt->move(10,120);
    }

    if (QGroupBox *group = groupByName(QLatin1String("Adjustment"))) {
        group->move(10,240);
        group->setFixedWidth(555);
        group->setFixedHeight(110);

        if (QWidget *wgt = widgetByName(QLatin1String("Brightness")))
            wgt->move(10,30);

        if (QWidget *wgt = widgetByName(QLatin1String("Sharpness")))
            wgt->move(200,30);

        if (QWidget *wgt = widgetByName(QLatin1String("Saturation")))
            wgt->move(380,30);
    }

    if (QWidget *wgt = widgetByName(QLatin1String("Equalize Brightness"))) {
        wgt->move(135,360);

        if (wgt = widgetByName(QLatin1String("Equalize Color")))
            wgt->move(340,360);

        if (wgt = widgetByName(QLatin1String("Rotate 180")))
            wgt->hide(); // for now
    } else {
        if (QWidget *wgt = widgetByName(QLatin1String("Rotate 180")))
            wgt->move(255,360);
    }
}

void AVSettingsDlg::initExposure()
{
    if (QWidget *wgt = widgetByName(QLatin1String("Illumination")))
        wgt->move(420,60);

    if (QWidget *wgt = widgetByName(QLatin1String("Lighting")))
        wgt->move(420,210);

    if (QGroupBox *group = groupByName(QLatin1String("Low Light Mode"))) {
        group->move(10,60);
        group->setFixedWidth(380);
        group->setFixedHeight(240);

        if (QWidget *wgt = widgetByName(QLatin1String("Light Mode")))
            wgt->move(30,30);

        if (QWidget *wgt = widgetByName(QLatin1String("Short Exposure")))
            wgt->move(200,75);

        if (QWidget *wgt = widgetByName(QLatin1String("Auto exposure On/Off")))
            wgt->move(30,200);
    }
}

void AVSettingsDlg::initAI()
{
    if (QWidget *wgt = widgetByName(QLatin1String("AutoIris enable")))
        wgt->move(200,180);
}

void AVSettingsDlg::initDN()
{
    if (QWidget *wgt = widgetByName(QLatin1String("Day/Night Mode")))
        wgt->move(215,130);
}

void AVSettingsDlg::initMD()
{
    if (QWidget *wgt = widgetByName(QLatin1String("Enable motion detection")))
        wgt->move(175,30);

    if (QWidget *wgt = widgetByName(QLatin1String("Zone size")))
        wgt->move(80,80);

    if (QWidget *wgt = widgetByName(QLatin1String("Threshold")))
        wgt->move(320,80);

    if (QWidget *wgt = widgetByName(QLatin1String("Sensitivity")))
        wgt->move(80,210);

    if (QWidget *wgt = widgetByName(QLatin1String("Md detail")))
        wgt->move(320,210);
}

void AVSettingsDlg::initAdmin()
{
    if (QWidget *wgt = widgetByName(QLatin1String("Factory defaults")))
        wgt->move(230,150);
}

void AVSettingsDlg::paramValueChanged(const QnParam &param)
{
    if (param.name() == QLatin1String("Light Mode") || param.name() == QLatin1String("Codec"))
        correctWgtsState();
}

void AVSettingsDlg::correctWgtsState()
{
    if (QWidget *wgt = widgetByName(QLatin1String("Light Mode"))) {
        const QString val = param(wgt).value().toString();
        if (wgt = widgetByName(QLatin1String("Short Exposure")))
            wgt->setVisible(val == QLatin1String("highspeed"));
    }

    if (QWidget *wgt = widgetByName(QLatin1String("Codec"))) {
        const QString val = param(wgt).value().toString();
        if (wgt = widgetByName(QLatin1String("Bitrate")))
            wgt->setVisible(val == QLatin1String("H.264"));
    }
}

void AVSettingsDlg::onSuggestions()
{
    QString text = tr("To reduce the bandwidth try to set Light Mode on Exposure tab to HightSpeed and set Short Exposure to 30ms.");
    QMessageBox mbox(QMessageBox::Information, tr("Suggestion"), text, QMessageBox::Ok, this, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    mbox.exec();
}
