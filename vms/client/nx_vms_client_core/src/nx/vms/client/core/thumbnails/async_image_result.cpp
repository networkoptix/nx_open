// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "async_image_result.h"

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

} // namespace nx::vms::client::core
