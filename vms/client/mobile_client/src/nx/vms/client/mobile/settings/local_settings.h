// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QObject>

#include <nx/reflect/instrument.h>
#include <nx/utils/json/qt_containers_reflect.h>
#include <nx/utils/property_storage/storage.h>

namespace nx::vms::client::mobile {

using UserDescriptor = QString; // User id or cloud login (for cloud conenction).
using LayoutDescriptor = QString; // Layout id or system id (for all cameras "layout").

class LocalSettings: public nx::utils::property_storage::Storage
{
    Q_OBJECT

public:
    static constexpr int kInvalidCameraLayoutCustomColumnsCount = -1;
    static constexpr int kDefaultCameraLayoutPosition = 0;

public:
    LocalSettings();

    SecureProperty<QMap<UserDescriptor, QMap<LayoutDescriptor, int> > >
        cameraLayoutCustomColumnCountsForUsers
            {this, "cameraLayoutCustomColumnCountsForUsers"};

    SecureProperty<QMap<UserDescriptor, QMap<LayoutDescriptor, int> > >
        cameraLayoutPositionsForUsers
            {this, "cameraLayoutPositionsForUsers"};

    int getCameraLayoutCustomColumnCount(
        const UserDescriptor& userDescriptor,
        const LayoutDescriptor& layoutDescriptor) const;

    // If columnsCount == kInvalidCameraLayoutCustomColumnsCount, removes stored value.
    void setCameraLayoutCustomColumnCount(
        const UserDescriptor& userDescriptor,
        const LayoutDescriptor& layoutDescriptor,
        int columnsCount);

    int getCameraLayoutPosition(
        const UserDescriptor& userDescriptor,
        const LayoutDescriptor& layoutDescriptor) const;

    // If position == kDefaultCameraLayoutPosition, removes stored value.
    void setCameraLayoutPosition(
        const UserDescriptor& userDescriptor,
        const LayoutDescriptor& layoutDescriptor,
        int position);
};

} // namespace nx::vms::client::mobile
