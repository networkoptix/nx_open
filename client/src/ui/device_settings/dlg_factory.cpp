#include "dlg_factory.h"

#include <QtCore/QList>

#include <utils/common/warnings.h>
#include "device_settings_dlg.h"


typedef QList<CLAbstractDlgManufacture *> Manufactures;
Q_GLOBAL_STATIC(Manufactures, manufactures)

bool CLDeviceSettingsDlgFactory::canCreateDlg(QnResourcePtr dev)
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

CLAbstractDeviceSettingsDlg *CLDeviceSettingsDlgFactory::createDlg(QnResourcePtr dev)
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
    if(manufacture == NULL) {
        qnNullWarning(manufacture);
        return;
    }

    manufactures()->append(manufacture);
}
