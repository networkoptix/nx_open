#ifndef arecon_dlg_factory_h1733
#define arecon_dlg_factory_h1733

#include "../../dlg_factory.h"
#include "../../device_settings_dlg.h"

class AreconVisionDlgManufacture : public CLAbstractDlgManufacture
{
public:
    AreconVisionDlgManufacture();

    QDialog *createDlg(QnResourcePtr resource);
    bool canProduceDlg(QnResourcePtr resource) const;

private:
    QList<QString> mPossibleNames;
};


class AVSettingsDlg : public CLAbstractDeviceSettingsDlg
{
    Q_OBJECT

public:
    AVSettingsDlg(QnResourcePtr resource, QWidget *parent = 0);

protected Q_SLOTS:
    virtual void onSuggestions();
    virtual void setParam(const QString& name, const QVariant& val);
    virtual void onClose();

private:
    void initTabsOrder();
    void initImageQuality();
    void initExposure();
    void initAI();
    void initDN();
    void initMD();
    void initAdmin();
    //==========
    void correctWgtsState();
};

#endif //arecon_dlg_factory_h1733
