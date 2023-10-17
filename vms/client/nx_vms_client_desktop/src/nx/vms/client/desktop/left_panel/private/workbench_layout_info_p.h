// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <ui/workbench/workbench_context_aware.h>

Q_MOC_INCLUDE("core/resource/layout_resource.h")
Q_MOC_INCLUDE("core/resource/videowall_resource.h")

namespace nx::vms::client::desktop {

class AbstractResourceContainer: public QObject
{
    Q_OBJECT
public:
    explicit AbstractResourceContainer(QObject* parent = nullptr): QObject(parent) {}
    Q_INVOKABLE virtual bool containsResource(QnResource* resource) const = 0;
};

class WorkbenchLayoutInfo:
    public QObject,
    public WindowContextAware
{
    Q_OBJECT
    Q_PROPERTY(QnLayoutResource* currentLayout READ currentLayout NOTIFY currentLayoutChanged)
    Q_PROPERTY(QnResource* currentResource READ currentResource NOTIFY currentResourceChanged)
    Q_PROPERTY(AbstractResourceContainer* resources READ resources NOTIFY resourcesChanged)
    Q_PROPERTY(QnVideoWallResource* reviewedVideoWall READ reviewedVideoWall
        NOTIFY currentLayoutChanged)
    Q_PROPERTY(QnVideoWallResource* controlledVideoWall READ controlledVideoWall
        NOTIFY currentLayoutChanged)
    Q_PROPERTY(QnUuid controlledVideoWallItemId READ controlledVideoWallItemId
        NOTIFY currentLayoutChanged)
    Q_PROPERTY(QnUuid reviewedShowreelId READ reviewedShowreelId NOTIFY currentLayoutChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)
    Q_PROPERTY(int maximumItemCount READ maximumItemCount CONSTANT)
    Q_PROPERTY(bool isLocked READ isLocked NOTIFY isLockedChanged)
    Q_PROPERTY(bool isPreviewSearchLayout READ isPreviewSearchLayout CONSTANT)
    Q_PROPERTY(bool isShowreelReviewLayout READ isShowreelReviewLayout CONSTANT)

public:
    WorkbenchLayoutInfo(WindowContext* context, QObject* parent = nullptr);

    QnLayoutResource* currentLayout() const;
    QnResource* currentResource() const;
    AbstractResourceContainer* resources() const;
    QnUuid reviewedShowreelId() const;
    QnVideoWallResource* reviewedVideoWall() const;
    QnVideoWallResource* controlledVideoWall() const;
    QnUuid controlledVideoWallItemId() const;

    int itemCount() const;
    int maximumItemCount() const;
    bool isLocked() const;
    bool isPreviewSearchLayout() const;
    bool isShowreelReviewLayout() const;

signals:
    void currentLayoutChanged();
    void currentResourceChanged();
    void resourcesChanged();
    void itemCountChanged();
    void isLockedChanged();

private:
    class ResourceContainer;
    ResourceContainer* const m_resources;
    mutable std::optional<QnVideoWallResourcePtr> m_controlledVideoWall;
};

} // namespace nx::vms::client::desktop
