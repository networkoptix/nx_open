// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <ui/workbench/workbench_context_aware.h>

class QPoint;

namespace nx::vms::client::desktop {

namespace menu {

class Manager;
class Parameters;

} // namespace menu

namespace ResourceTree { enum class ActivationType; }

class ResourceTreeInteractionHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    using ActionManager = nx::vms::client::desktop::menu::Manager;

    explicit ResourceTreeInteractionHandler(QnWorkbenchContext* context, QObject* parent = nullptr);
    virtual ~ResourceTreeInteractionHandler() override;

    /**
     * Shows Resource Tree context menu.
     * @param parent Suggested parent for context menu which will be shown.
     * @param globalPos Cursor position in global coordinates system.
     * @param index The index belonging to the item view model on which context menu was requested.
     * @param selection Item view model indexes of currently selected items.
     */
    void showContextMenu(QWidget* parent, const QPoint& globalPos, const QModelIndex& index,
        const QModelIndexList& selection);

    /**
     * Handler which is called either when user presses Enter key while interacting with
     * Resource Tree via keyboard or performs mouse double click on some item.
     * @param index The index of the view model item on which action has been performed.
     *     It is supposed to be current index in selection model terms.
     * @param selection The list of selected indexes
     * @param activationType The type of item activation. Possible values:
     *  - enterKey: the item is activated by the Enter key;
     *  - doubleClick: the item is activated by double clicking;
     *  - middleClick: the item is activated by pressing the middle button.
     */
    void activateItem(const QModelIndex& index,
        const QModelIndexList& selection,
        const ResourceTree::ActivationType activationType,
        const Qt::KeyboardModifiers modifiers = Qt::NoModifier);

    /**
     * Handler which is called when user presses Enter key from resource search input field.
     * @param indexes The indexes of the filtering model representing search results.
     * @param modifiers Keyboard modifiers pressed along with Enter key.
     */
    void activateSearchResults(const QModelIndexList& indexes, Qt::KeyboardModifiers modifiers);

    /**
     * Method exposed only for QnResourceBrowserWidget implementation of menu::TargetProvider
     * interface.
     * @note Probably it will be removed in the future.
     * @param index Item view's current index.
     * @param selection Item view model indexes of currently selected items.
     * @returns Action parameters for given item view state.
     */
    menu::Parameters actionParameters(const QModelIndex& index,
        const QModelIndexList& selection);

signals:
    void editRequested();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
