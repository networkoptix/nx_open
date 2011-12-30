#include "dlg_factory.h"

#include <QtCore/QList>

#include <utils/common/warnings.h>
#include "device_settings_dlg.h"

typedef QList<CLAbstractDlgManufacture *> Manufactures;
Q_GLOBAL_STATIC(Manufactures, manufactures)

bool CLDeviceSettingsDlgFactory::canCreateDlg(QnResourcePtr resource)
{
    if (!resource)
        return false;

    foreach (CLAbstractDlgManufacture *man, *manufactures())
    {
        if (man->canProduceDlg(resource))
            return true;
    }

    return false;
}

CLAbstractDeviceSettingsDlg *CLDeviceSettingsDlgFactory::createDlg(QnResourcePtr resource)
{
    if (!resource)
        return 0;

    foreach (CLAbstractDlgManufacture *man, *manufactures())
    {
        if (man->canProduceDlg(resource))
            return man->createDlg(resource);
    }

    return 0;
}

void CLDeviceSettingsDlgFactory::registerDlgManufacture(CLAbstractDlgManufacture *manufacture)
{
    if (!manufacture) {
        qnNullWarning(manufacture);
        return;
    }

    manufactures()->append(manufacture);
}
