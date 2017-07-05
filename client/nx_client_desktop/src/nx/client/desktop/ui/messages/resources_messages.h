#pragma once

#include <text/tr_functions.h>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>
#include <client/client_globals.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace resources {

QN_DECLARE_TR_FUNCTIONS("nx::client::desktop::ui::resources")

void layoutAlreadyExists(QWidget* parent);

bool overrideLayout(QWidget* parent);
bool overrideLayoutTour(QWidget* parent);

bool changeUserLocalLayout(QWidget* parent, const QnResourceList& stillAccessible);
bool addToRoleLocalLayout(QWidget* parent, const QnResourceList& toShare);
bool removeFromRoleLocalLayout(QWidget* parent, const QnResourceList& stillAccessible);
bool sharedLayoutEdit(QWidget* parent);
bool stopSharingLayouts(QWidget* parent, const QnResourceList& mediaResources,
    const QnResourceAccessSubject& subject);
bool deleteSharedLayouts(QWidget* parent, const QnResourceList& layouts);
bool deleteLocalLayouts(QWidget* parent, const QnResourceList& stillAccessible);

bool removeItemsFromLayout(QWidget* parent, const QnResourceList& resources);

bool changeVideoWallLayout(QWidget* parent, const QnResourceList& inaccessible);

bool deleteResources(QWidget* parent, const QnResourceList& resources);

} // namespace resources
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
