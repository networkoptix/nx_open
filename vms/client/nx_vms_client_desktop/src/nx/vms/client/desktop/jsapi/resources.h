// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

class QnWorkbenchContext;

namespace nx::vms::client::desktop::jsapi {

namespace detail { class ResourcesApiBackend; }

/** Class proxies calls from a JS script to the resource management backend. */
class Resources: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    Resources(QObject* parent = nullptr);

    virtual ~Resources() override;

    /**
     * Checks if the Resource with the specified id can have a media stream.
     * @ingroup vms-resources
     */
    Q_INVOKABLE bool hasMediaStream(const QString& resourceId) const;

    /** List of all available (to user and by the API constraints) resources. */
    Q_INVOKABLE QJsonArray resources() const;

    /** Description of the resource with the specified identifier. */
    Q_INVOKABLE QJsonObject resource(const QString& resourceId) const;

signals:
    void added(const QJsonObject& resource);
    void removed(const QString& resourceId);

private:
    nx::utils::ImplPtr<detail::ResourcesApiBackend> d;
};

} // namespace nx::vms::client::desktop::jsapi
