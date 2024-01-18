// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QRegularExpression>

#include <core/resource/camera_bookmark_fwd.h>

namespace nx::vms::common {

class NX_VMS_COMMON_API BookmarkTextFilter
{
public:
    BookmarkTextFilter(const QString& text);

    bool match(const CameraBookmark& bookmark) const;
    bool operator()(const CameraBookmark& bookmark) const;

private:
    std::vector<std::pair<QString, QRegularExpression>> m_words;
};

} // namespace nx::vms::common
