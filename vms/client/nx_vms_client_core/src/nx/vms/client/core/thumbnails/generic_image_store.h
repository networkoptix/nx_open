// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

#include "abstract_image_source.h"

namespace nx::vms::client::core {

class GenericImageStore: public AbstractImageSource
{
public:
    using AbstractImageSource::AbstractImageSource; //< Forward constructors.

    virtual QImage image(const QString& imageId) const override;

    void addImage(const QString& imageId, const QImage& image);
    QString addImage(const QImage& image);

    void removeImage(const QString& imageId);

private:
    QHash<QString, QImage> m_images;
};

} // namespace nx::vms::client::core
