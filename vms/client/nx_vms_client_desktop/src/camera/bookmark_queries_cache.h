// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <QtCore/QObject>

#include <camera/camera_bookmarks_manager_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>

class QnBookmarkQueriesCache: public QObject
{
public:
    void clear();

    bool hasQuery(const QnVirtualCameraResourcePtr& camera) const;

    QnCameraBookmarksQueryPtr getQuery(const QnVirtualCameraResourcePtr& camera) const;
    QnCameraBookmarksQueryPtr getOrCreateQuery(const QnVirtualCameraResourcePtr& camera);

    void removeQueryByCamera(const QnVirtualCameraResourcePtr& camera);

    void setQueriesActive(bool active);

private:
    std::map<QnVirtualCameraResourcePtr, QnCameraBookmarksQueryPtr> m_queries;
};
