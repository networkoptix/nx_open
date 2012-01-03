#ifndef device_settings_tab_h_2004
#define device_settings_tab_h_2004

#include <core/resource/resource.h>

class CLAbstractDeviceSettingsDlg;

class CLDeviceSettingsTab : public QWidget
{
    Q_OBJECT

public:
    CLDeviceSettingsTab(CLAbstractDeviceSettingsDlg *dialog, QnResourcePtr resource, const QString &group);
    ~CLDeviceSettingsTab();

    QString name() const;

protected:
    CLAbstractDeviceSettingsDlg *const m_dialog;
    const QnResourcePtr m_resource;
    const QString m_group;
};

#endif //abstract_device_settings_tab_h_2004
