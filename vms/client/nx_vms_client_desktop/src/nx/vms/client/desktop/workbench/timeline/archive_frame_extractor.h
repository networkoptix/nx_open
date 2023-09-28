// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>

#include <core/resource/resource_fwd.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/impl_ptr.h>
#include <utils/media/frame_info.h>

namespace nx::vms::client::desktop {

/**
 * The ArchiveFrameExtractor class provides asynchronous interface for extracting single frames
 * at given time points from resources which have video content.
 */
class ArchiveFrameExtractor: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    enum class VideoQuality
    {
        high,
        low,
    };

    enum class ResultCode
    {
        decoded,
        decodeFailed,
        noData,
        endReached,
        aborted,
    };

    struct Request
    {
        std::chrono::milliseconds timePoint;
        std::chrono::milliseconds tolerance;
        QVariant userData;
    };

    struct Result
    {
        Request request;
        ResultCode resultCode;
        CLVideoDecoderOutputPtr decodedFrame;
    };

public:
    /**
     * @param mediaResource Valid pointer to the QnMediaResource interface which provide resource
     *     with video content. It could be a camera resource or local video file resource.
     * @param credentials Credentials to access media.
     * @param videoQuality Video quality parameter allows to choose between high quality and low
     *     quality media streams if such exists.
     */
    ArchiveFrameExtractor(
        const QnMediaResourcePtr& mediaResource,
        VideoQuality videoQuality,
        bool sleepIfEmptySocket = false);

    virtual ~ArchiveFrameExtractor() override;

    /**
     * @return The size to which the resulting frame will be explicitly scaled. Default value is
     *     is invalid QSize, that means that no scaling will be applied to the resulting frame and
     *     it will be returned as is.
     */
    QSize scaleSize();

    /**
     * @param size If valid QSize, than resulting frames will be explicitly scaled to that size,
     *     ignoring aspect ratio of an original frame.
     */
    void setScaleSize(const QSize& size);

    /**
     * Places frame request at the end of the queue, requests are processed in the first in, first
     * out order.
     * @param request Specifies the timestamp of the frame to be extracted from the archive as well
     *     as allowable error.
     */
    void requestFrame(const Request& request);

    /**
     * Clears all pending frame requests from the queue and aborts any active frame extraction.
     * That's not guaranteed that results from previous requests won't be received after that call
     * when control will be returned to the event loop since they could be already enqueued.
     */
    void clearRequestQueue();

signals:

    /**
     * Signal which emitted when result is ready. Signal is emitted from the this object's thread
     * event loop.
     */
    void frameRequestDone(ArchiveFrameExtractor::Result result);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
