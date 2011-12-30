#include "dlg_factory.h"

#include "device_settings_dlg.h"

Q_GLOBAL_STATIC(CLDeviceSettingsDlgFactory, factory)

CLDeviceSettingsDlgFactory::~CLDeviceSettingsDlgFactory()
{
    qDeleteAll(manufactures);
}

CLDeviceSettingsDlgFactory *CLDeviceSettingsDlgFactory::instance()
{
    return factory();
}

bool CLDeviceSettingsDlgFactory::canCreateDlg(QnResourcePtr resource)
{
    if (!resource)
        return false;

    foreach (CLAbstractDlgManufacture *manufacture, factory()->manufactures) {
        if (manufacture->canProduceDlg(resource))
            return true;
    }

    return false;
}

CLAbstractDeviceSettingsDlg *CLDeviceSettingsDlgFactory::createDlg(QnResourcePtr resource)
{
    if (!resource)
        return 0;

    foreach (CLAbstractDlgManufacture *manufacture, factory()->manufactures) {
        if (manufacture->canProduceDlg(resource))
            return manufacture->createDlg(resource);
    }

    return 0;
}

void CLDeviceSettingsDlgFactory::registerDlgManufacture(CLAbstractDlgManufacture *manufacture)
{
    if (manufacture)
        factory()->manufactures.append(manufacture);
}
