#pragma once

#include <bitset>

#include <QtWidgets/QTreeView>

#include <ui/widgets/common/tree_view.h>

#include <core/resource/resource_fwd.h>

class QSortFilterProxyModel;
class QnResourceListModel;

class QnResourceListView: public QnTreeView
{
    using base_type = QnTreeView;
public:
    enum Option
    {
        HideStatus,
        ServerAsHealthMonitor,
        SortByName,
        SortAsInTree,

        Count
    };
    using Options = std::bitset<(int)Option::Count>;

    /* If parent is specified, the view creates snapped vertical scroll bar at window edge.
     * If parent is not specified, the view uses normal vertical scroll bar. */
    explicit QnResourceListView(QWidget* parent = nullptr);

    /** Default constructor. Will set SortAsInTree as options. */
    QnResourceListView(const QnResourceList& resources, QWidget* parent = nullptr);

    QnResourceListView(const QnResourceList& resources, Options options, QWidget* parent = nullptr);

    QnResourceList resources() const;
    void setResources(const QnResourceList& resources);

    Options options() const;
    void setOptions(Options value);

    bool isSelectionEnabled() const;
    void setSelectionEnabled(bool value);

    QnResourcePtr selectedResource() const;

protected:
    virtual QSize sizeHint() const override;

private:
    void resetSortModel(QSortFilterProxyModel* model);

private:
    QnResourceListModel* m_model = nullptr;
    QSortFilterProxyModel* m_sortModel = nullptr;
    Options m_options;
};
