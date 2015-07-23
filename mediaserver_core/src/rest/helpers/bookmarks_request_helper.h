#pragma once

#include <core/resource/camera_bookmark_fwd.h>

struct QnBookmarkRequestData;

class QnBookmarksRequestHelper
{
public:
    static QnCameraBookmarkList load(const QnBookmarkRequestData& request);
};

