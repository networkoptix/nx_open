#ifndef device_settings_tab_h_2004
#define device_settings_tab_h_2004

#include <QtGui/QWidget>

class CLAbstractDeviceSettingsDlg;
class QnParamList;

class CLDeviceSettingsTab : public QWidget
{
    Q_OBJECT

public:
    CLDeviceSettingsTab(CLAbstractDeviceSettingsDlg *dialog, const QnParamList &paramList, const QString &group);
    ~CLDeviceSettingsTab();

    QString name() const;

protected:
    CLAbstractDeviceSettingsDlg *const m_dialog;
    const QString m_group;
};

#endif //abstract_device_settings_tab_h_2004
