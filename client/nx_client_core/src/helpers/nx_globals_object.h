#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtQuick/QQuickItem>

#include <nx/utils/uuid.h>
#include <helpers/url_helper.h>
#include <utils/common/software_version.h>

class NxGlobalsObject: public QObject
{
    Q_OBJECT

public:
    NxGlobalsObject(QObject* parent = nullptr);

    Q_INVOKABLE QnUrlHelper url(const QUrl& url) const;
    Q_INVOKABLE QnSoftwareVersion softwareVersion(const QString& version) const;

    Q_INVOKABLE void ensureFlickableChildVisible(QQuickItem* item);

    Q_INVOKABLE QnUuid uuid(const QString& uuid) const;
};
