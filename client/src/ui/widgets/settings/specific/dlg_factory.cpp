#include "dlg_factory.h"

#include <QtCore/QList>

#include "device_settings_dlg.h"

#include "plugins/arecontvision/arecont_dlg.h"

class DefaultDlgManufacture : public CLAbstractDlgManufacture
{
public:
    inline bool canProduceDlg(QnResourcePtr resource) const
    { return resource->checkFlags(QnResource::live_cam); }
    inline QDialog *createDlg(QnResourcePtr resource, QWidget *parent)
    { return new CLAbstractDeviceSettingsDlg(resource, parent); }
};

typedef QList<CLAbstractDlgManufacture *> Manufactures;

Q_GLOBAL_STATIC(Manufactures, manufactures)


void CLDeviceSettingsDlgFactory::initialize()
{
    CLDeviceSettingsDlgFactory::registerDlgManufacture(new DefaultDlgManufacture); // TODO: leaking allocated objects
    CLDeviceSettingsDlgFactory::registerDlgManufacture(new AreconVisionDlgManufacture);
}

bool CLDeviceSettingsDlgFactory::canCreateDlg(const QnResourcePtr &resource)
{
    if (!resource)
        return false;

    foreach (CLAbstractDlgManufacture *manufacture, *manufactures()) {
        if (manufacture->canProduceDlg(resource))
            return true;
    }

    return false;
}

QDialog *CLDeviceSettingsDlgFactory::createDlg(const QnResourcePtr &resource, QWidget *parent)
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
        manufactures()->prepend(manufacture);
}
