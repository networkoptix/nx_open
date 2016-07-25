#pragma once

#include <QtWidgets/QTreeView>

#include <core/resource/resource_fwd.h>

class QnResourceListModel;

class QnResourceListView: public QTreeView
{
    using base_type = QTreeView;
public:
    QnResourceListView(QWidget* parent = nullptr);

    QnResourceList resources() const;
    void setResources(const QnResourceList& resources);

private:
    QnResourceListModel* m_model;
};
