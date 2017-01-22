#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>
#include <client/client_globals.h>

//TODO: #GDM standartize naming
class QnLayoutsHandlerMessages
{
    Q_DECLARE_TR_FUNCTIONS(QnLayoutsHandlerMessages)
public:
    static void layoutAlreadyExists(QWidget* parent);

    /**
    * @brief askOverrideLayout     Show message box asking user if he really wants to override existing layout.
    * @return                      Selected button (Yes or Cancel)
    */
    static QDialogButtonBox::StandardButton askOverrideLayout(QWidget* parent);

    static bool changeUserLocalLayout(QWidget* parent, const QnResourceList& stillAccessible);
    static bool addToRoleLocalLayout(QWidget* parent, const QnResourceList& toShare);
    static bool removeFromRoleLocalLayout(QWidget* parent, const QnResourceList& stillAccessible);
    static bool sharedLayoutEdit(QWidget* parent);
    static bool stopSharingLayouts(QWidget* parent, const QnResourceList& mediaResources,
        const QnResourceAccessSubject& subject);
    static bool deleteSharedLayouts(QWidget* parent, const QnResourceList& layouts);
    static bool deleteLocalLayouts(QWidget* parent, const QnResourceList& stillAccessible);

    static bool changeVideoWallLayout(QWidget* parent, const QnResourceList& inaccessible);

private:

    static bool showCompositeDialog(
        QWidget* parent,
        Qn::ShowOnceMessage showOnceFlag,
        const QString& text,
        const QString& extras = QString(),
        const QnResourceList& resources = QnResourceList(),
        bool useResources = true);
};
