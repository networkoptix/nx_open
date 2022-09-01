// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <bitset>

#include <QtWidgets/QTreeView>

#include <nx/vms/client/desktop/common/widgets/tree_view.h>

#include <core/resource/resource_fwd.h>

class QSortFilterProxyModel;
class QnResourceListModel;

class QnResourceListView: public nx::vms::client::desktop::TreeView
{
    using base_type = nx::vms::client::desktop::TreeView;
public:
    enum Option
    {
        HideStatusOption            = 0x01,
        ServerAsHealthMonitorOption = 0x02,
        SortByNameOption            = 0x04,
        SortAsInTreeOption          = 0x08,
    };
    Q_DECLARE_FLAGS(Options, Option)

    /* If parent is specified, the view creates snapped vertical scroll bar at window edge.
     * If parent is not specified, the view uses normal vertical scroll bar. */
    explicit QnResourceListView(QWidget* parent = nullptr);

    /** Default constructor. Will set SortAsInTreeOption as options. */
    explicit QnResourceListView(const QnResourceList& resources, QWidget* parent = nullptr);

    QnResourceListView(const QnResourceList& resources, Options options, QWidget* parent = nullptr);

    QnResourceList resources() const;
    void setResources(const QnResourceList& resources);

    Options options() const;
    void setOptions(Options value);

    bool isSelectionEnabled() const;
    void setSelectionEnabled(bool value);

    QnResourcePtr selectedResource() const;

    QnResourceListModel* model() const;

protected:
    virtual QSize sizeHint() const override;

private:
    void resetSortModel(QSortFilterProxyModel* model);

private:
    QnResourceListModel* m_model = nullptr;
    QSortFilterProxyModel* m_sortModel = nullptr;
    Options m_options;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnResourceListView::Options)
