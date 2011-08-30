#ifndef dlg_factory__h_1712
#define dlg_factory__h_1712

class CLDevice;
class CLAbstractDeviceSettingsDlg;

class CLAbstractDlgManufacture
{
protected:
    virtual ~CLAbstractDlgManufacture() {}

public:
    virtual CLAbstractDeviceSettingsDlg *createDlg(CLDevice *dev) = 0;
    virtual bool canProduceDlg(CLDevice *dev) const = 0;
};

class CLDeviceSettingsDlgFactory
{
public:
    static CLAbstractDeviceSettingsDlg *createDlg(CLDevice *dev);
    static void registerDlgManufacture(CLAbstractDlgManufacture *manufacture);
};

#endif //dlg_factory__h_1712
