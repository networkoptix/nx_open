// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

Q_MOC_INCLUDE("core/resource/resource.h")

class QnCameraAccessRightsHelper:
    public QObject
{
    Q_OBJECT
    Q_PROPERTY(QnResource* resource READ rawResource WRITE setRawResource NOTIFY resourceChanged)
    Q_PROPERTY(bool canViewArchive READ canViewArchive NOTIFY canViewArchiveChanged)
    Q_PROPERTY(bool canManagePtz READ canManagePtz NOTIFY canManagePtzChanged)
public:
    explicit QnCameraAccessRightsHelper(QObject *parent = nullptr);
    ~QnCameraAccessRightsHelper();

    bool canViewArchive() const;

    bool canManagePtz() const;

private:
    QnResource* rawResource() const;
    void setRawResource(QnResource* value);

signals:
    void resourceChanged();
    void canViewArchiveChanged();
    void canManagePtzChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
