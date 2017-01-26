#pragma once

#include <QtWidgets/QTreeView>

#include <ui/widgets/common/tree_view.h>

#include <core/resource/resource_fwd.h>

class QnResourceListModel;

class QnResourceListView: public QnTreeView
{
    using base_type = QnTreeView;
public:
    explicit QnResourceListView(QWidget* parent = nullptr);
    QnResourceListView(const QnResourceList& resources, QWidget* parent = nullptr);
    QnResourceListView(const QnResourceList& resources, bool simplify,
        QWidget* parent = nullptr);

    QnResourceList resources() const;
    void setResources(const QnResourceList& resources);

    /** Simplified view hides resource statuses and displays servers as health monitors. */
    bool isSimplified() const;
    void setSimplified(bool value);

protected:
    virtual QSize sizeHint() const override;

private:
    QnResourceListModel* m_model;
};
