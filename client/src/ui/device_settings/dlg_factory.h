#ifndef dlg_factory__h_1712
#define dlg_factory__h_1712

#include "core/resource/resource.h"

class QnResource;
class CLAbstractDeviceSettingsDlg;

/**
 * Interface for creating device-specific settings dialogs.
 */
class CLAbstractDlgManufacture
{
public:
    /**
     * \param dev                       Resource to create settings dialog for.
     * \returns                         Newly created settings dialog,
     *                                  or NULL if it could not be created.
     */
    virtual CLAbstractDeviceSettingsDlg *createDlg(QnResourcePtr dev) = 0;

    /**
     * \param dev                       Resource to check.
     * \returns                         Whether this manufacture can create a
     *                                  settings dialog for the given resource.
     */
    virtual bool canProduceDlg(QnResourcePtr dev) const = 0;

protected:
    /**
     * Virtual destructor.
     */
    virtual ~CLAbstractDlgManufacture() {}
};


/**
 * Factory that can be used to create a settings dialog given a resource.
 */
class CLDeviceSettingsDlgFactory
{
public:
    /**
     * \param dev                       Resource to check.
     * \returns                         Whether this factory can create a
     *                                  settings dialog for the given resource.
     */
    static bool canCreateDlg(QnResourcePtr dev);

    /**
     * \param dev                       Resource to create settings dialog for.
     * \returns                         Newly created settings dialog for the
     *                                  given resource, or NULL if the dialog
     *                                  could not be created.
     */
    static CLAbstractDeviceSettingsDlg *createDlg(QnResourcePtr dev);

    /**
     * Registers a dialog manufacture with this factory.
     *
     * \param manufacture               Manufacture to register.
     */
    static void registerDlgManufacture(CLAbstractDlgManufacture *manufacture);
};

#endif //dlg_factory__h_1712
