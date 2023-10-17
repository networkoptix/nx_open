// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <memory>

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/abstract_entity.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class DetailedResourceTreeWidget; }

namespace nx::vms::client::desktop {

namespace entity_item_model { class EntityItemModel; }
namespace entity_resource_tree { class ResourceTreeEntityBuilder; }

class ResourceTreeIconDecoratorModel;
class FilteredResourceViewWidget;
class ResourceDetailsWidget;

class DetailedResourceTreeWidget: public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    using AbstractEntityPtr = entity_item_model::AbstractEntityPtr;
    using ResourceTreeEntityBuilder = entity_resource_tree::ResourceTreeEntityBuilder;

    using EntityFactoryFunction = std::function<AbstractEntityPtr(const ResourceTreeEntityBuilder*)>;
    using ResourceFilter = std::function<bool(const QnResourcePtr& resource)>;

    DetailedResourceTreeWidget(int columnCount, QWidget* parent);
    virtual ~DetailedResourceTreeWidget() override;

    /**
     * @param entityFactoryFunction Function which creates root entity for the tree view model.
     *     Root entity will be created immediately and re-created on every user session change.
     *     Setting null function wrapper clears tree view model.
     */
    void setTreeEntityFactoryFunction(const EntityFactoryFunction& entityFactoryFunction);

    bool displayResourceStatus() const;
    void setDisplayResourceStatus(bool display);

    /**
     * @return True if tree view model has zero top level rows.
     */
    bool isEmpty() const;

    /**
     * Count of unique resources displayed in the tree view.
     */
    int resourceCount();

    FilteredResourceViewWidget* resourceViewWidget() const;

    /**
     * Details panel located at the left side may display additional information as camera preview,
     * resource name, hints and warnings.
     */
    ResourceDetailsWidget* detailsPanelWidget() const;

    using DetailsPanelUpdateFunction =
        std::function<void(const QModelIndex&, ResourceDetailsWidget*)>;

    /**
     * @param updateFunction Function wrapper which will be called whenever hovered tree view item
     *     index changed. Default implementation sets resource acquired by the Qn::ResourceRole
     *     from the sibling of hovered index at column 0 to the details panel camera preview, and
     *     text acquired by the Qt::DisplayRole from the same index to the details panel caption
     *     label. If null function wrapper will be passed as parameter, default implementation
     *     will be used.
     */
    void setDetailsPanelUpdateFunction(const DetailsPanelUpdateFunction& updateFunction);

    /**
     * @return whether or not right details panel is hidden.
     */
    bool isDetailsPanelHidden() const;

    /**
     * @param value True if details panel is hidden.
     */
    void setDetailsPanelHidden(bool value);

protected:
    virtual QAbstractItemModel* model() const;
    virtual void showEvent(QShowEvent* event) override;
    virtual void setupHeader();

private:
    void updateDetailsPanel();

private:
    const std::unique_ptr<Ui::DetailedResourceTreeWidget> ui;
    const std::unique_ptr<entity_resource_tree::ResourceTreeEntityBuilder> m_treeEntityBuilder;
    EntityFactoryFunction m_entityFactoryFunction;
    entity_item_model::AbstractEntityPtr m_treeEntity;
    const std::unique_ptr<entity_item_model::EntityItemModel> m_entityModel;
    const std::unique_ptr<ResourceTreeIconDecoratorModel> m_iconDecoratorModel;
    DetailsPanelUpdateFunction m_detailsPanelUpdateFunction;
};

} // namespace nx::vms::client::desktop
