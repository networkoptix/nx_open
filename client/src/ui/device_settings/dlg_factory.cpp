#include "dlg_factory.h"

#include <QtCore/QList>

#include "device_settings_dlg.h"

typedef QList<CLAbstractDlgManufacture *> Manufactures;

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

QDialog *CLDeviceSettingsDlgFactory::createDlg(QnResourcePtr resource, QWidget *parent)
{
    if (!resource)
        return 0;

    foreach (CLAbstractDlgManufacture *manufacture, *manufactures()) {
        if (manufacture->canProduceDlg(resource))
            return manufacture->createDlg(resource, parent);
    }

    return 0;
}

void CLDeviceSettingsDlgFactory::registerDlgManufacture(CLAbstractDlgManufacture *manufacture)
{
    if (manufacture)
        manufactures()->append(manufacture);
}
