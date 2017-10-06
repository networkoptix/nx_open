#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtQuick/QQuickItem>

#include <nx/utils/uuid.h>
#include <helpers/url_helper.h>
#include <utils/common/software_version.h>
#include <nx/client/core/enums.h>

namespace nx {
namespace client {
namespace core {

class NxGlobalsObject: public QObject
{
    Q_OBJECT
    Q_ENUMS(nx::client::core::Enums::ResourceFlags)

public:
    NxGlobalsObject(QObject* parent = nullptr);

    Q_INVOKABLE QnUrlHelper url(const QUrl& url) const;
    Q_INVOKABLE QnSoftwareVersion softwareVersion(const QString& version) const;

    Q_INVOKABLE void ensureFlickableChildVisible(QQuickItem* item);

    Q_INVOKABLE QnUuid uuid(const QString& uuid) const;
};

} // namespace core
} // namespace client
} // namespace nx
