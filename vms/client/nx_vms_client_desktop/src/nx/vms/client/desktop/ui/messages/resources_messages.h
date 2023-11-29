// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace messages {

class Resources
{
    Q_DECLARE_TR_FUNCTIONS(Resources)
public:
    static void layoutAlreadyExists(QWidget* parent);

    static bool overrideLayout(QWidget* parent);
    static bool overrideShowreel(QWidget* parent);

    static bool sharedLayoutEdit(QWidget* parent);

    static bool deleteLayouts(QWidget* parent, const QnResourceList& sharedLayouts,
        const QnResourceList& personalLayouts);

    static bool removeItemsFromLayout(QWidget* parent, const QnResourceList& resources);
    static bool removeItemsFromShowreel(QWidget* parent, const QnResourceList& resources);

    static bool changeVideoWallLayout(QWidget* parent, const QnResourceList& inaccessible);

    static bool deleteResources(
        QWidget* parent,
        const QnResourceList& resources,
        bool allowSilent = true);

    static bool deleteResourcesFailed(QWidget* parent, const QnResourceList& resources);

    static bool deleteVideoWallItems(QWidget* parent, const QnVideoWallItemIndexList& items);
    static bool deleteVideoWallMatrices(QWidget* parent, const QnVideoWallMatrixIndexList& matrices);

    static bool stopVirtualCameraUploads(QWidget* parent, const QnResourceList& resources);

    static bool mergeResourceGroups(QWidget* parent, const QString& groupName);

    static bool moveProxiedWebPages(QWidget* parent, const QnResourceList& resources,
        const QnResourcePtr& server);

    static bool moveCamerasToServer(
        QWidget* parent,
        const QnVirtualCameraResourceList& nonMovableCameras,
        const QString& targetServerName,
        const QString& currentServerName,
        bool hasVirtualCameras,
        bool hasUsbCameras);

    static void warnCamerasCannotBeMoved(
        QWidget* parent,
        bool hasVirtualCameras,
        bool hasUsbCameras);
};

} // namespace messages
} // namespace ui
} // namespace nx::vms::client::desktop
