// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>
#include <core/resource/resource.h>

#include <business/business_resource_validation.h>

#include <nx/vms/client/desktop/resource_dialogs/detailed_resource_tree_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/models/tooltip_decorator_model.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_selection_dialogs_globals.h>

namespace nx::vms::client::desktop {

class ResourceSelectionWidget: public DetailedResourceTreeWidget
{
    Q_OBJECT
    using base_type = DetailedResourceTreeWidget;

public:
    using ResourceValidator = std::function<bool(const QnResourcePtr& resource)>;
    using AlertTextProvider = std::function<QString(const QSet<QnResourcePtr>& resources)>;

    /**
     * @param resourceValidator Unary predicate which defines whether resource is valid choice for
     *     selection or not.
     */
    ResourceSelectionWidget(
        const ResourceValidator& resourceValidator,
        const AlertTextProvider& alertTextProvider,
        QWidget* parent);
    virtual ~ResourceSelectionWidget() override;

    /**
     * @return Dialog selection mode, default value is <tt>MultiSelection</tt>.
     */
    ResourceSelectionMode selectionMode() const;

    /**
     * Setups dialog selection mode, selected resources are reseted if selection mode changed.
     */
    void setSelectionMode(ResourceSelectionMode mode);

    /**
     * @return True if recording indicator is shown for displayed cameras and devices. Recording
     *     indicator hasn't shown by default.
     */
    bool showRecordingIndicator() const;

    /**
     * Sets whether recording indicator is shown or not for displayed cameras and devices.
     */
    void setShowRecordingIndicator(bool show);

    /**
     * Shows the invalid devices
     */
    bool showInvalidResources() const;

    /**
     * Sets show invalid devices
     */
    void setShowInvalidResources(bool show);

    /**
     * @return True if validator predicate has been provided and returns false for one of the
     *     selected resources.
     */
    bool selectionHasInvalidResources() const;

    /**
     * @return Set of resources pointers representing selected resources.
     */
    QSet<QnResourcePtr> selectedResources() const;

    /**
     * Sets selected resources.
     */
    void setSelectedResources(const QSet<QnResourcePtr>& selectedResources);

    /**
     * @return Set containing selected resources IDs.
     */
    QnUuidSet selectedResourcesIds() const;

    /**
     * @param  resourcesIds List of identifiers for the resources.
     */
    void setSelectedResourcesIds(const QnUuidSet& resourcesIds);

    /**
     * Convenience getter which may be used instead of <tt>selectedResources()</tt> if
     * <tt>ResourceSelectionMode::SingleSelection</tt> or
     * <tt>ResourceSelectionMode::ExclusiveSelection</tt> selection mode used.
     */
    QnResourcePtr selectedResource() const;

    /**
     * Convenience setter which may be used instead of <tt>setSelectedResources()</tt> if
     * <tt>ResourceSelectionMode::SingleSelection</tt> or
     * <tt>ResourceSelectionMode::ExclusiveSelection</tt> selection mode used.
     */
    void setSelectedResource(const QnResourcePtr& resource);

    /**
     * Convenience getter which may be used instead of <tt>selectedResourcesIds()</tt> if
     * <tt>ResourceSelectionMode::SingleSelection</tt> or
     * <tt>ResourceSelectionMode::ExclusiveSelection</tt> selection mode used.
     */
    QnUuid selectedResourceId() const;

    /**
     * Convenience setter which may be used instead of <tt>setSelectedResourcesIds()</tt> if
     * <tt>ResourceSelectionMode::SingleSelection</tt> or
     * <tt>ResourceSelectionMode::ExclusiveSelection</tt> selection mode used.
     */
    void setSelectedResourceId(const QnUuid& resourceId);

    void setToolTipProvider(TooltipDecoratorModel::TooltipProvider tooltipProvider);

    void setItemsEnabled(bool enabled);

signals:
    void selectionModeChanged(ResourceSelectionMode mode);
    void selectionChanged();

protected:
    virtual QAbstractItemModel* model() const override final;
    virtual void showEvent(QShowEvent* event) override;

private:
    struct Private;
    const std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop
