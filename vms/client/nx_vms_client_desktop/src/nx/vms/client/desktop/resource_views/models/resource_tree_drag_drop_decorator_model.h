// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QIdentityProxyModel>
#include <QtCore/QPointer>

#include <core/resource/shared_resource_pointer_list.h>

class QnResourcePool;
class QnResource;

namespace nx::vms::client::desktop {

namespace ui::action { class Manager; }
namespace ui::workbench { class ActionHandler; }

class ResourceTreeSettings;

/**
 * Decorator model which implements Resource Tree drag & drop behavior. Logic has been extracted
 * from QnResourceTreeModel and kept unchanged, except that some clean-up, structuring and
 * proofreading has been made. Implementation does not exceed bounds of QAbstractItemModel
 * interface, that means that it's suitable for application with both old and new Resource Tree
 * model implementations.
 */
class ResourceTreeDragDropDecoratorModel: public QIdentityProxyModel
{
    Q_OBJECT
    using base_type = QIdentityProxyModel;

public:
    ResourceTreeDragDropDecoratorModel(
        QnResourcePool* resourcePool,
        ui::action::Manager* actionManager,
        ui::workbench::ActionHandler* actionHandler,
        ResourceTreeSettings* resourceTreeSettings);

    virtual ~ResourceTreeDragDropDecoratorModel() override;

    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
    virtual QStringList mimeTypes() const override;
    virtual QMimeData* mimeData(const QModelIndexList& indexes) const override;
    virtual bool canDropMimeData(const QMimeData* mimeData, Qt::DropAction action,
        int row, int column, const QModelIndex& parent) const override;

    virtual bool dropMimeData(const QMimeData* mimeData, Qt::DropAction action,
        int row, int column, const QModelIndex& parent) override;

    virtual Qt::DropActions supportedDropActions() const override;

    bool isWebPageDragDropEnabled() const;

private:
    QModelIndex targetDropIndex(const QModelIndex& dropIndex) const;

    void moveCustomGroup(
        QnSharedResourcePointerList<QnResource> sourceResources,
        const QString& dragGroupId,
        const QString& dropGroupId);

    QnResourcePool* resourcePool() const;
    ui::action::Manager* actionManager() const;

private:
    QnResourcePool* m_resourcePool = nullptr;
    ui::action::Manager* m_actionManager = nullptr;
    ui::workbench::ActionHandler* m_actionHandler = nullptr;
    QPointer<ResourceTreeSettings> m_resourceTreeSettings;
};

} // namespace nx::vms::client::desktop
