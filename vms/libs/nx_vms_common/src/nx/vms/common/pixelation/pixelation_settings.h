// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <utility>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

namespace nx::vms::common {

class SystemContext;

/**
 * Pixelation Settings takes into account the current taxonomy, settings and permissions.
 */
class NX_VMS_COMMON_API PixelationSettings: public QObject
{
    Q_OBJECT

public:
    PixelationSettings(SystemContext* context, QObject* parent = nullptr);
    ~PixelationSettings();

    bool isPixelationRequired(
        const QnUserResourcePtr& user,
        const nx::Uuid& cameraId,
        const QString& objectTypeId = {}) const;

    bool isAllTypes() const;
    QSet<QString> objectTypeIds() const;
    double intensity() const;

signals:
    void settingsChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::common
