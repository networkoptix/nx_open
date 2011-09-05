#include "dlg_factory.h"

#include <QtCore/QList>

#include "device_settings_dlg.h"

typedef QList<CLAbstractDlgManufacture *> Manufactures;
Q_GLOBAL_STATIC(Manufactures, manufactures)

bool CLDeviceSettingsDlgFactory::canCreateDlg(CLDevice *dev)
{
    if (!dev)
        return false;

    foreach (CLAbstractDlgManufacture *man, *manufactures())
    {
        if (man->canProduceDlg(dev))
            return true;
    }

    return false;
}

CLAbstractDeviceSettingsDlg *CLDeviceSettingsDlgFactory::createDlg(CLDevice *dev)
{
    if (!dev)
        return 0;

    foreach (CLAbstractDlgManufacture *man, *manufactures())
    {
        if (man->canProduceDlg(dev))
            return man->createDlg(dev);
    }

    return 0;
}

void CLDeviceSettingsDlgFactory::registerDlgManufacture(CLAbstractDlgManufacture *manufacture)
{
    if (manufacture)
        manufactures()->append(manufacture);
}
