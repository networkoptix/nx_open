#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>

//TODO: #GDM standartize naming
class QnLayoutsHandlerMessages
{
    Q_DECLARE_TR_FUNCTIONS(QnLayoutsHandlerMessages)
public:
    static void layoutAlreadyExists(QWidget* parent);

    /**
    * @brief askOverrideLayout     Show message box asking user if he really wants to override existing layout.
    * @param buttons               Message box buttons.
    * @param defaultButton         Default button.
    * @return                      Selected button.
    */
    static QDialogButtonBox::StandardButton askOverrideLayout(
        QWidget* parent,
        QDialogButtonBox::StandardButtons buttons,
        QDialogButtonBox::StandardButton defaultButton);

    static bool changeUserLocalLayout(QWidget* parent, const QnResourceList& stillAccessible);
    static bool addToRoleLocalLayout(QWidget* parent, const QnResourceList& toShare);
    static bool removeFromRoleLocalLayout(QWidget* parent, const QnResourceList& stillAccessible);
    static bool sharedLayoutEdit(QWidget* parent);
    static bool stopSharingLayouts(QWidget* parent, const QnResourceList& mediaResources,
        const QnResourceAccessSubject& subject);
    static bool deleteSharedLayouts(QWidget* parent, const QnResourceList& layouts);
    static bool deleteLocalLayouts(QWidget* parent, const QnResourceList& stillAccessible);
};
