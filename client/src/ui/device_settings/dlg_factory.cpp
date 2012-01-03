#include "dlg_factory.h"

#include <QtCore/QList>

#include "device_settings_dlg.h"

class Manufactures : public QList<CLAbstractDlgManufacture *>
{
public:
    ~Manufactures() { qDeleteAll(*this); }
};

Q_GLOBAL_STATIC(Manufactures, manufactures)


bool CLDeviceSettingsDlgFactory::canCreateDlg(QnResourcePtr resource)
{
    if (!resource)
        return false;

    foreach (CLAbstractDlgManufacture *manufacture, *manufactures()) {
        if (manufacture->canProduceDlg(resource))
            return true;
    }

    return false;
}

CLAbstractDeviceSettingsDlg *CLDeviceSettingsDlgFactory::createDlg(QnResourcePtr resource)
{
    if (!resource)
        return 0;

    foreach (CLAbstractDlgManufacture *manufacture, *manufactures()) {
        if (manufacture->canProduceDlg(resource))
            return manufacture->createDlg(resource);
    }

    return 0;
}

void CLDeviceSettingsDlgFactory::registerDlgManufacture(CLAbstractDlgManufacture *manufacture)
{
    if (manufacture)
        manufactures()->append(manufacture);
}
