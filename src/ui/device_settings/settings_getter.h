#ifndef dlg_settings_getter_h2355
#define dlg_settings_getter_h2355
#include "device\device_command_processor.h"
#include "base\associativearray.h"

class CLAbstractSettingsWidget;

class CLDeviceGetParamCommand : public QObject, public CLDeviceCommand
{
    Q_OBJECT
public:
    CLDeviceGetParamCommand(CLAbstractSettingsWidget* wgt);
    void execute();
signals:
    void ongetvalue(QString val);
private:
    CLAbstractSettingsWidget* m_wgt;
};

#endif //dlg_settings_getter_h2355