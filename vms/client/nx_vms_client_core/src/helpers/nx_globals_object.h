#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtQuick/QQuickItem>

#include <nx/utils/uuid.h>
#include <nx/utils/url.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/client/core/enums.h>

namespace nx::vms::client::core {

class NxGlobalsObject: public QObject
{
    Q_OBJECT
    Q_ENUMS(nx::vms::client::core::Enums::ResourceFlags)

public:
    NxGlobalsObject(QObject* parent = nullptr);

    Q_INVOKABLE nx::utils::Url url(const QString& url) const;
    Q_INVOKABLE nx::utils::Url url(const QUrl& url) const;
    Q_INVOKABLE nx::vms::api::SoftwareVersion softwareVersion(const QString& version) const;

    Q_INVOKABLE void ensureFlickableChildVisible(QQuickItem* item);

    Q_INVOKABLE QnUuid uuid(const QString& uuid) const;
};

} // namespace nx::vms::client::core
