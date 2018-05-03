#pragma once

#include <QtCore/QCoreApplication>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>
#include <client/client_globals.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace messages {

class Resources
{
    Q_DECLARE_TR_FUNCTIONS(Resources)
public:
    static void layoutAlreadyExists(QWidget* parent);

    static bool overrideLayout(QWidget* parent);
    static bool overrideLayoutTour(QWidget* parent);

    static bool changeUserLocalLayout(QWidget* parent, const QnResourceList& stillAccessible);
    static bool addToRoleLocalLayout(QWidget* parent, const QnResourceList& toShare);
    static bool removeFromRoleLocalLayout(QWidget* parent, const QnResourceList& stillAccessible);
    static bool sharedLayoutEdit(QWidget* parent);
    static bool stopSharingLayouts(QWidget* parent, const QnResourceList& mediaResources,
        const QnResourceAccessSubject& subject);
    static bool deleteSharedLayouts(QWidget* parent, const QnResourceList& layouts);
    static bool deleteLocalLayouts(QWidget* parent, const QnResourceList& stillAccessible);

    static bool removeItemsFromLayout(QWidget* parent, const QnResourceList& resources);
    static bool removeItemsFromLayoutTour(QWidget* parent, const QnResourceList& resources);

    static bool changeVideoWallLayout(QWidget* parent, const QnResourceList& inaccessible);

    static bool deleteResources(QWidget* parent, const QnResourceList& resources);

    static bool stopWearableUploads(QWidget* parent, const QnResourceList& resources);
};

} // namespace messages
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
