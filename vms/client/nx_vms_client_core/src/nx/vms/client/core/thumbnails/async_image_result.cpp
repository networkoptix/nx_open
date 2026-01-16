// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "async_image_result.h"

#include <QtCore/QScopedPointerDeleteLater>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::core {

const QString AsyncImageResult::kTimestampUsKey = "timestampUs";

AsyncImageResult::AsyncImageResult(QObject* parent):
    QObject(parent)
{
}

void AsyncImageResult::setImage(const QImage& image)
{
    NX_ASSERT(m_image.isNull());
    m_image = image;
    m_ready = true;
    emit ready(QPrivateSignal());
}

void AsyncImageResult::setError(const QString& errorString)
{
    m_errorString = errorString;
    setImage({});
}

std::chrono::microseconds AsyncImageResult::timestamp(const QImage& image)
{
    if (image.isNull())
        return std::chrono::microseconds(-1);

    const auto timestampText = image.text(kTimestampUsKey);
    return std::chrono::microseconds(timestampText.toLongLong());
}

void AsyncImageResult::setTimestamp(QImage& image, std::chrono::microseconds value)
{
    image.setText(kTimestampUsKey, QString::number(value.count()));
}

QFuture<std::tuple<QImage, QString>> AsyncImageResult::toFuture(
    std::unique_ptr<AsyncImageResult> asyncResult)
{
    QPromise<std::tuple<QImage, QString>> promise;
    promise.start();

    if (asyncResult->isReady()) //< Result was obtained synchronously.
    {
        promise.emplaceResult(asyncResult->image(), asyncResult->errorString());
        promise.finish();
        return promise.future();
    }

    auto future = promise.future();

    std::unique_ptr<AsyncImageResult, QScopedPointerDeleteLater> request(asyncResult.release());
    const auto sender = request.get(); //< Need to get a raw copy before `request` is moved.

    QObject::connect(sender, &AsyncImageResult::ready,
        [promise = std::move(promise), request = std::move(request)]() mutable
        {
            promise.emplaceResult(request->image(), request->errorString());
            promise.finish();
        });

    return future;
}

} // namespace nx::vms::client::core
