// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <business/business_resource_validation.h>
#include <core/resource/resource.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/resource_dialogs/detailed_resource_tree_widget.h>
#include <nx/vms/client/desktop/resource_dialogs/resource_dialogs_constants.h>

namespace nx::vms::client::desktop {

class ResourceSelectionWidget: public DetailedResourceTreeWidget
{
    Q_OBJECT
    using base_type = DetailedResourceTreeWidget;

public:
    using ResourceValidator = std::function<bool(const QnResourcePtr& resource)>;
    using AlertTextProvider =
        std::function<QString(const QSet<QnResourcePtr>& resources, bool pinnedItemSelected)>;

    /**
     * @param parent Valid pointer to the parent widget, should be initialized
     *     QnWorkbenchContextAware descendant.
     * @param columnCount Number of columns in the encapsulated resource tree model. Check mark
     *      column will be the last one.
     * @param resourceValidator Unary predicate which defines whether resource is valid choice for
     *     selection or not.
     * @param alertTextProvider Function wrapper which holds function that provides alert text in
     *     the case if some invalid resources are checked. List of invalid resources will be passed
     *     as parameter.
     */
    ResourceSelectionWidget(
        QWidget* parent,
        int columnCount = resource_selection_view::ColumnCount,
        ResourceValidator resourceValidator = {},
        AlertTextProvider alertTextProvider = {});
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

    /** Returns whether pinned item is selected. */
    bool pinnedItemSelected() const;

    /** Set pinned item selected state. */
    void setPinnedItemSelected(bool selected);

    /**
     * @return Set containing selected resources IDs.
     */
    UuidSet selectedResourcesIds() const;

    /**
     * @param  resourcesIds List of identifiers for the resources.
     */
    void setSelectedResourcesIds(const UuidSet& resourcesIds);

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
    nx::Uuid selectedResourceId() const;

    /**
     * Convenience setter which may be used instead of <tt>setSelectedResourcesIds()</tt> if
     * <tt>ResourceSelectionMode::SingleSelection</tt> or
     * <tt>ResourceSelectionMode::ExclusiveSelection</tt> selection mode used.
     */
    void setSelectedResourceId(const nx::Uuid& resourceId);

signals:
    void selectionModeChanged(ResourceSelectionMode mode);
    void selectionChanged();

protected:
    virtual QAbstractItemModel* model() const override;
    virtual void setupHeader() override;
    virtual void showEvent(QShowEvent* event) override;

private:
    struct Private;
    const std::unique_ptr<Private> d;
};

} // namespace nx::vms::client::desktop
