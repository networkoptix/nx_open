#ifndef arecon_dlg_factory_h1733
#define arecon_dlg_factory_h1733

#include "../../dlg_factory.h"
#include "../../device_settings_dlg.h"

class AreconVisionDlgManufacture : public CLAbstractDlgManufacture
{
public:
    AreconVisionDlgManufacture();

    bool canProduceDlg(QnResourcePtr resource) const;
    QDialog *createDlg(QnResourcePtr resource, QWidget *parent);

private:
    QList<QString> mPossibleNames;
};


class AVSettingsDlg : public CLAbstractDeviceSettingsDlg
{
    Q_OBJECT

public:
    AVSettingsDlg(QnResourcePtr resource, QWidget *parent = 0);

public Q_SLOTS:
    void accept();

protected Q_SLOTS:
    void buildTabs();

private Q_SLOTS:
    void paramValueChanged(const QnParam &param);
    void correctWgtsState();
    void onSuggestions();

private:
    void initImageQuality();
    void initExposure();
    void initAI();
    void initDN();
    void initMD();
    void initAdmin();
};

#endif //arecon_dlg_factory_h1733
