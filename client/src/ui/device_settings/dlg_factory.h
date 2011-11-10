#ifndef dlg_factory__h_1712
#define dlg_factory__h_1712

#include "core/resource/resource.h"


class QnResource;
class CLAbstractDeviceSettingsDlg;

class CLAbstractDlgManufacture
{
protected:
    virtual ~CLAbstractDlgManufacture() {}

public:
    virtual CLAbstractDeviceSettingsDlg *createDlg(QnResourcePtr dev) = 0;
    virtual bool canProduceDlg(QnResourcePtr dev) const = 0;
};


class CLDeviceSettingsDlgFactory
{
public:
    static bool canCreateDlg(QnResourcePtr dev);
    static CLAbstractDeviceSettingsDlg *createDlg(QnResourcePtr dev);
    static void registerDlgManufacture(CLAbstractDlgManufacture *manufacture);
};

#endif //dlg_factory__h_1712
