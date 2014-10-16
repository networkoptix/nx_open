#ifndef CAMERAS_MODEL_H
#define CAMERAS_MODEL_H

#include <QtCore/QAbstractListModel>

#include <utils/common/uuid.h>
#include <core/resource/resource_fwd.h>

class QnPlainResourceModelNode;

class QnPlainResourceModel : public QAbstractListModel {
    Q_OBJECT

    Q_ENUMS(NodeType)

public:
    enum NodeType {
        ServerNode,
        CameraNode
    };

    explicit QnPlainResourceModel(QObject *parent = 0);

    virtual int rowCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual QHash<int, QByteArray> roleNames() const;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent) const override;

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_resourcePool_resourceChanged(const QnResourcePtr &resource);

private:
    QModelIndex index(QnPlainResourceModelNode *node) const;
    QnPlainResourceModelNode *existentNode(const QnResourcePtr &resource);
    QnPlainResourceModelNode *node(const QnResourcePtr &resource);
    QnPlainResourceModelNode *node(const QModelIndex &index) const;

private:
    QHash<QnUuid, QnPlainResourceModelNode*> m_nodeById;
    QList<QnUuid> m_itemOrder;
};

#endif // CAMERAS_MODEL_H
