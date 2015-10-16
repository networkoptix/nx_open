#pragma once

#include <core/resource/camera_bookmark_fwd.h>

struct QnGetBookmarksRequestData;

class QnBookmarksRequestHelper
{
public:
    static QnCameraBookmarkList load(const QnGetBookmarksRequestData& request);
};

