#ifndef dlg_settings_getter_h2355
#define dlg_settings_getter_h2355

#include "core/resource/resource_command_consumer.h"

class CLAbstractSettingsWidget;

class QnDeviceGetParamCommand : public QObject, public QnResourceCommand
{
    Q_OBJECT
public:
    QnDeviceGetParamCommand(CLAbstractSettingsWidget *wgt);
    void execute();

Q_SIGNALS:
    void ongetvalue(QString val);

private:
    CLAbstractSettingsWidget *const m_wgt;
};

typedef QSharedPointer<QnDeviceGetParamCommand> QnDeviceGetParamCommandPtr;

#endif //dlg_settings_getter_h2355
