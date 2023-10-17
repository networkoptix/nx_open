// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QItemSelectionModel>
#include <QtQuick/QQuickItem>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/utils/qml_helpers.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/ui/scene/models/resource_tree_model_adapter.h>
#include <nx/vms/client/desktop/utils/qml_property.h>
#include <ui/workbench/workbench_context_aware.h>

#include "workbench_layout_info_p.h"

using namespace nx::vms::client::core;

namespace nx::vms::client::desktop {

class LeftPanelWidget;

class ResourceTreeWrapper: public QmlProperty<QQuickItem*>
{
public:
    using QmlProperty::QmlProperty;

    const QmlProperty<ResourceTreeModelAdapter*> model{this, "model"};
    const QmlProperty<QModelIndex> currentIndex{this, "currentIndex"};
    const QmlProperty<QVariant> scene{this, "scene"};

    QModelIndexList selection() const
    {
        return nx::utils::toTypedQList<QModelIndex>(
            invokeQmlMethod<QVariantList>(*this, "selection"));
    }

    void setSelection(const QModelIndexList& indexes) const
    {
        invokeQmlMethod<void>(*this, "setSelection", indexes);
    }

    void clearSelection() const
    {
        invokeQmlMethod<void>(*this, "clearSelection");
    }

    void setExpanded(const QModelIndex& index, bool value) const
    {
        invokeQmlMethod<void>(*this, "setExpanded", index, value);
    }

    void setCurrentIndex(const QModelIndex& index,
        QItemSelectionModel::SelectionFlags command = QItemSelectionModel::ClearAndSelect) const
    {
        invokeQmlMethod<void>(*this, "setCurrentIndex", index, (int) command);
    }

    void startEditing() const
    {
        invokeQmlMethod<void>(*this, "startEditing");
    }
};

class ResourceBrowserWrapper:
    public QObject,
    public WindowContextAware
{
public:
    Q_OBJECT

    const QmlProperty<QQuickItem*> resourceBrowser;
    const ResourceTreeWrapper tree{&resourceBrowser, "tree"};

public:
    ResourceBrowserWrapper(
        WindowContext* context,
        QmlProperty<QQuickItem*> resourceBrowser,
        QWidget* focusScope);

    virtual ~ResourceBrowserWrapper() override;

    menu::Parameters currentParameters() const;

signals:
    void resourceSelectionChanged();

private:
    void clearResourceSelection();
    void handleNewResourceItemAction();
    void handleSelectAllAction();
    void beforeGroupProcessing();
    void afterGroupProcessing(menu::IDType eventType);
    void acquireClientState();

    std::pair<QModelIndex, int /*depth*/> findResourceIndex(const QModelIndex& parent) const;

    void saveGroupExpandedState(const QModelIndexList& within);
    void restoreGroupExpandedState(const QModelIndex& under);

private:
    class StateDelegate;
    QSet<ResourceTree::ExpandedNodeId> expandedNodeIds;

    nx::utils::ImplPtr<WorkbenchLayoutInfo> m_layoutInfo;
    bool m_inSelection = false;

    // A widget that should be focused when a QML item is about to be focused.
    QPointer<QWidget> m_focusScope;

    // Special information to keep selection & expanded state while processing resource groups.
    QPersistentModelIndex m_nestedResourceIndex;
    int m_nestedResourceLevel = -1; //< Relative to a group being processed.
    QHash<QnResourcePtr, int /*depth*/> m_groupExpandedState;
};

} // namespace nx::vms::client::desktop
