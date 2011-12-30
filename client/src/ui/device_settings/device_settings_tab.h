#ifndef device_settings_tab_h_2004
#define device_settings_tab_h_2004

#include <core/resource/resource.h>

class CLAbstractDeviceSettingsDlg;

class CLDeviceSettingsTab : public QWidget
{
    Q_OBJECT

public:
    CLDeviceSettingsTab(CLAbstractDeviceSettingsDlg *dlg, QObject *handler, QnResourcePtr dev, QString group);
    ~CLDeviceSettingsTab();

    QString name() const;

protected:
    CLAbstractDeviceSettingsDlg *const mDlg;
    QObject *const mHandler;
    const QnResourcePtr mDevice;
    QString mGroup;
};

#endif //abstract_device_settings_tab_h_2004
