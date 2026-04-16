// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_bookmark.h>

#include "abstract_object_data.h"

namespace nx::vms::client::mobile {
namespace timeline {

class BookmarkData: public AbstractObjectData
{
    Q_OBJECT
public:
    BookmarkData(const common::CameraBookmark& bookmark, const QnResourcePtr& resource);

    virtual nx::Uuid id() const override;
    virtual qint64 startTimeMs() const override;
    virtual qint64 durationMs() const override;
    virtual QString title() const override;
    virtual QString description() const override;
    virtual QString imagePath() const override;
    virtual QVariant tags() const override;
    virtual QVariant attributes() const override;
    virtual QnResourcePtr resource() const override;

    virtual common::CameraBookmark convertToBookmark() const override;
    void update(common::CameraBookmark bookmark);

    static MultiObjectData merge(
        std::span<const common::CameraBookmark> bookmarks,
        const QnTimePeriod& period,
        std::chrono::milliseconds minimumStackDuration,
        int limit,
        const QnResourcePtr& resource);

private:
    common::CameraBookmark m_bookmark;
    QString m_imagePath;
    QnResourcePtr m_resource;
};

} // namespace timeline
} // namespace nx::vms::client::mobile
