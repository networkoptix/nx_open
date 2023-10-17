// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QIdentityProxyModel>

// Squish does not handle forward declarations well, so resource_tree_squish_facade.h is included.
#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/ui/scene/models/resource_tree_squish_facade.h>

Q_MOC_INCLUDE("nx/vms/client/desktop/window_context.h")

class QnResourceTreeSortProxyModel;

namespace nx::vms::client::desktop {

class WindowContext;

class ResourceTreeModelAdapter: public ScopedModelOperations<QIdentityProxyModel>
{
    Q_OBJECT
    Q_PROPERTY(nx::vms::client::desktop::WindowContext* context
        READ context WRITE setContext NOTIFY contextChanged)
    Q_PROPERTY(nx::vms::client::desktop::ResourceTree::FilterType filterType
        READ filterType WRITE setFilterType NOTIFY filterTypeChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(bool isFiltering READ isFiltering NOTIFY isFilteringChanged)
    Q_PROPERTY(QModelIndex rootIndex READ rootIndex NOTIFY rootIndexChanged)
    Q_PROPERTY(QVector<nx::vms::client::desktop::ResourceTree::ShortcutHint> shortcutHints
        READ shortcutHints NOTIFY shortcutHintsChanged)
    Q_PROPERTY(bool localFilesMode READ localFilesMode NOTIFY localFilesModeChanged)
    Q_PROPERTY(bool extraInfoRequired READ isExtraInfoRequired NOTIFY extraInfoRequiredChanged)
    Q_PROPERTY(nx::vms::client::desktop::ResourceTreeModelSquishFacade* squishFacade
        READ squishFacade)

    using base_type = ScopedModelOperations<QIdentityProxyModel>;

public:
    ResourceTreeModelAdapter(QObject* parent = nullptr);
    virtual ~ResourceTreeModelAdapter() override;

    WindowContext* context() const;
    void setContext(WindowContext* context);

    Q_INVOKABLE bool isFilterRelevant(nx::vms::client::desktop::ResourceTree::FilterType type) const;

    // Save/restore a set of "super-persistent" indices, for example an expand/collapse state.
    // Passed indices are mapped to and stored as the most underlying model persistent indices.
    Q_INVOKABLE void pushState(const QModelIndexList& state);
    Q_INVOKABLE QModelIndexList popState();

    Q_INVOKABLE void activateItem(const QModelIndex& index,
        const QModelIndexList& selection,
        const nx::vms::client::desktop::ResourceTree::ActivationType activationType,
        const Qt::KeyboardModifiers modifiers);

    Q_INVOKABLE void showContextMenu(
        const QPoint& globalPos, const QModelIndex& index, const QModelIndexList& selection);

    Q_INVOKABLE void activateSearchResults(Qt::KeyboardModifiers modifiers);

    Q_INVOKABLE bool isExtraInfoForced(QnResource* resource) const;

    Q_INVOKABLE bool expandsOnDoubleClick(const QModelIndex& index) const;

    Q_INVOKABLE bool activateOnSingleClick(const QModelIndex& index) const;

    bool isExtraInfoRequired() const;

    menu::Parameters actionParameters(
        const QModelIndex& index, const QModelIndexList& selection) const;

    ResourceTree::FilterType filterType() const;
    void setFilterType(ResourceTree::FilterType value);

    QString filterText() const;
    void setFilterText(const QString& value);

    bool isFiltering() const;

    QModelIndex rootIndex() const;
    bool localFilesMode() const;

    QVector<ResourceTree::ShortcutHint> shortcutHints() const;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

    ResourceTree::ExpandedNodeId expandedNodeId(const QModelIndex& index) const;
    void setAutoExpandedNodes(const std::optional<QSet<ResourceTree::ExpandedNodeId>>& value,
        bool signalModelReset = true);

    ResourceTreeModelSquishFacade* squishFacade();

    static void registerQmlType();

signals:
    void contextChanged();
    void filterTypeChanged();
    void filterTextChanged();
    void filterAboutToBeChanged(QPrivateSignal);
    void filterChanged(QPrivateSignal);
    void isFilteringChanged();
    void rootIndexChanged();
    void localFilesModeChanged();
    void editRequested();
    void extraInfoRequiredChanged();
    void shortcutHintsChanged();
    void saveExpandedState();
    void restoreExpandedState();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // nx::vms::client::desktop
