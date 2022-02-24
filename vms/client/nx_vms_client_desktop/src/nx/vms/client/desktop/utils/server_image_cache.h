// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/utils/server_file_cache.h>

#include <QtGui/QImage>
#include <utils/common/aspect_ratio.h>

namespace nx::vms::client::desktop {

class ServerImageCache: public ServerFileCache
{
    Q_OBJECT

    typedef ServerFileCache base_type;
public:
    explicit ServerImageCache(QObject *parent = 0);
    virtual ~ServerImageCache();

    QSize getMaxImageSize() const;

    static QString cachedImageFilename(const QString &sourcePath);

    void storeImage(const QString &filePath, const QnAspectRatio& aspectRatio = QnAspectRatio());

private slots:
    void at_imageConverted(const QString &filePath);

};

} // namespace nx::vms::client::desktop
