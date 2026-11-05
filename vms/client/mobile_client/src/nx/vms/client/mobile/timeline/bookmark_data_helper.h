// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <core/resource/camera_bookmark.h>

#include "object_data.h"

namespace nx::vms::client::mobile {
namespace timeline {

class BookmarkDataHelper
{
    Q_DECLARE_TR_FUNCTIONS(nx::vms::client::mobile::timeline::BookmarkDataHelper)

public:
    static ObjectData makeObjectData(
        const common::CameraBookmark& bookmark, const QnResourcePtr& resource);

    static QString iconPath();
};

} // namespace timeline
} // namespace nx::vms::client::mobile
