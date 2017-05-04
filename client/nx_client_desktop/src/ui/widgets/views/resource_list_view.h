#pragma once

#include <QtWidgets/QTreeView>

#include <ui/widgets/common/tree_view.h>

#include <core/resource/resource_fwd.h>

class QnResourceListModel;

class QnResourceListView: public QnTreeView
{
    using base_type = QnTreeView;
public:
    /* If parent is specified, the view creates snapped vertical scroll bar at window edge.
     * If parent is not specified, the view uses normal vertical scroll bar. */
    explicit QnResourceListView(QWidget* parent = nullptr);
    QnResourceListView(const QnResourceList& resources, QWidget* parent = nullptr);
    QnResourceListView(const QnResourceList& resources, bool simplify,
        QWidget* parent = nullptr);

    QnResourceList resources() const;
    void setResources(const QnResourceList& resources);

    /** Simplified view hides resource statuses and displays servers as health monitors. */
    bool isSimplified() const;
    void setSimplified(bool value);

    bool isSelectionEnabled() const;
    void setSelectionEnabled(bool value);

    QnResourcePtr selectedResource() const;

protected:
    virtual QSize sizeHint() const override;

private:
    QnResourceListModel* m_model;
};
