// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <network/base_system_description.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <nx/vms/client/desktop/workbench/state/workbench_state.h>

Q_MOC_INCLUDE("nx/vms/client/desktop/window_context.h")
Q_MOC_INCLUDE("ui/workbench/workbench_layout.h")

class QnWorkbenchLayout;
class QnWorkbenchGridMapper;
class QnWorkbenchItem;
class QQuickItem;

namespace nx::vms::client::desktop {

struct LogonData;

/**
 * Workbench ties layout, items and current UI-related "state" together.
 *
 * It abstracts away the fact that layout can be changed "on the fly" by
 * providing copies of layout signals that remove the necessity to
 * watch for layout changes.
 *
 * It also ensures that current layout is never nullptr by replacing nullptr layout
 * supplied by the user with internally stored empty layout.
 *
 * Workbench state consists of:
 * <ul>
 * <li>A list of layouts that are currently loaded.</li>
 * <li>Current layout that defines how items are placed.</li>
 * <li>A grid mapper that maps integer layout coordinates into floating-point
 *     surface coordinates.</li>
 * <li>Currently raised item - an item that is enlarged and is shown on top of other items.</li>
 * <li>Currently zoomed item - an item that is shown in full screen.</li>
 * </ul>
 */
class Workbench: public QObject, public WindowContextAware
{
    Q_OBJECT
    Q_PROPERTY(QnWorkbenchLayout* currentLayout
        READ currentLayout
        WRITE setCurrentLayout
        NOTIFY currentLayoutChanged)
    Q_PROPERTY(nx::vms::client::desktop::WindowContext* context READ windowContext CONSTANT)

public:
    Workbench(WindowContext* windowContext, QObject* parent = nullptr);
    virtual ~Workbench() override;

    /**
     * Size of a single unit of workbench grid, in scene coordinates. This basically is the width
     * of a single video item in scene coordinates.
     * Graphics scene has problems with handling mouse events on small scales, so the larger this
     * number, the better.
     */
    static constexpr qreal kUnitSize = 10000.0;

    /**
     * Clears this workbench by setting all of its properties to their initial
     * values.
     * Note that this function does not reset parameters of the grid mapper.
     */
    void clear();

    /**
     * Note that this function never returns nullptr.
     * @return Layout of this workbench.
     */
    QnWorkbenchLayout* currentLayout() const;

    /**
     * Resource for the current layout.
     * Note that this function never returns nullptr.
     */
    LayoutResourcePtr currentLayoutResource() const;

    /**
     * @return Index of the current layout. May return -1 if dummy layout is currently in use.
     */
    int currentLayoutIndex() const { return layoutIndex(currentLayout()); }

    /**
     * @param index Index of the layout to get.
     * @return Layout for the given index.
     */
    QnWorkbenchLayout* layout(int index) const;

    /** Find existing layout, corresponding to the provided resource. */
    QnWorkbenchLayout* layout(const LayoutResourcePtr& resource);

    /**
     * @return All layouts of this workbench. May be empty.
     */
    const std::vector<std::unique_ptr<QnWorkbenchLayout>>& layouts() const;

    /**
     * Create a new layout in this workbench.
     */
    QnWorkbenchLayout* addLayout(const LayoutResourcePtr& resource);

    /**
     * Create a new layout in this workbench and insert it to the given position.
     */
    QnWorkbenchLayout* insertLayout(const LayoutResourcePtr& resource, int index);

    /**
     * If provided layout based on 'replaceableLayout' is opened on the workbench, it is replaced
     * with the new WorkbenchLayout, based on the `newLayout` resource.
     * @return Newly created layout if any, nullptr otherwise.
     */
    QnWorkbenchLayout* replaceLayout(
        const LayoutResourcePtr& replaceableLayout,
        const LayoutResourcePtr& newLayout);

    /**
     * Remove this resource's layout if it exists.
     */
    void removeLayout(const LayoutResourcePtr& resource);

    /**
     * Remove thes resources' layouts if exists.
     */
    void removeLayouts(const LayoutResourceList& resources);

    /**
     * @param index Index of the layout to remove from the list of this workbench's layouts.
     */
    void removeLayout(int index);

    /**
     * @param layout Layout to move to a new position in the list of this workbench's layouts.
     * @param index New position for the given layout.
     */
    void moveLayout(QnWorkbenchLayout* layout, int index);

    /**
     * @param layout Layout to find in the list of this workbench's layouts.
     * @return Index of the given layout in the list of this workbench's layouts, or -1 if it is
     *     not there.
     */
    int layoutIndex(QnWorkbenchLayout* layout) const;

    /**
     * Note that workbench does not take ownership of the supplied layout.
     * If supplied layout is not in this workbench's layout list,
     * it will be added to it.
     * @param layout New layout for this workbench. If nullptr is specified, a new empty layout
     *     will be created.
     */
    void setCurrentLayout(QnWorkbenchLayout* layout);

    /**
     * Sets the index of the current layout. Note that index does not need to be valid as it will
     * be bounded to the closest valid value.
     * @param index New current layout index.
     */
    void setCurrentLayoutIndex(int index);

    void addSystem(QnUuid systemId, const LogonData& logonData);
    void addSystem(const QString& systemId, const LogonData& logonData);

    /**
     * @return Grid mapper for this workbench.
     */
    QnWorkbenchGridMapper* mapper() const;

    /**
     * @param role Role to get item for.
     * @return Item for the given item role.
     */
    QnWorkbenchItem* item(Qn::ItemRole role);

    /**
     * @param role Role to set an item for.
     * @param item New item for the given role.
     */
    void setItem(Qn::ItemRole role, QnWorkbenchItem* item);

    void update(const WorkbenchState& state);
    void submit(WorkbenchState& state);
    void applyLoadedState();
    bool isInLayoutChangeProcess() const;
    bool isInSessionRestoreProcess() const;

signals:
    /**
     * This signal is emitted whenever the layout of this workbench is about to be changed.
     */
    void currentLayoutAboutToBeChanged();

    /**
     * This signal is emitted whenever the layout of this workbench changes.
     *
     * In most cases there is no need to listen to this signal as
     * workbench emits <tt>itemRemoved</tt> signal for each of the
     * old layout items and <tt>itemAdded</tt> for each of the new
     * layout items when layout is changed.
     */
    void currentLayoutChanged();

    /**
     * Emitted when an item is added to the current layout.
     */
    void currentLayoutItemAdded(QnWorkbenchItem* item);

    /**
     * Emitted when an item is removed from the current layout.
     */
    void currentLayoutItemRemoved(QnWorkbenchItem* item);

    /**
     * Emitted when an item is added to or removed from the current layout.
     */
    void currentLayoutItemsChanged();

    void layoutsChanged();

    /**
     * This signal is emitted whenever a new item is about to be assigned to the role.
     *
     * @param role Item role.
     */
    void itemAboutToBeChanged(Qn::ItemRole role);

    /**
     * This signal is emitted whenever a new item is assigned to the role.
     *
     * @param role Item role.
     */
    void itemChanged(Qn::ItemRole role);

    /**
     * This signal is emitted whenever current cell aspect ratio changes.
     */
    void cellAspectRatioChanged();

    /**
     * This signal is emitted whenever current cell spacing changes.
     */
    void cellSpacingChanged();

    void currentSystemChanged(QnSystemDescriptionPtr systemDescription);

private slots:
    void at_layout_itemAdded(QnWorkbenchItem* item);
    void at_layout_itemRemoved(QnWorkbenchItem* item);
    void at_layout_cellAspectRatioChanged();
    void at_layout_cellSpacingChanged();

    void updateSingleRoleItem();
    void updateActiveRoleItem(const QnWorkbenchItem* removedItem = nullptr);
    void updateCentralRoleItem();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
