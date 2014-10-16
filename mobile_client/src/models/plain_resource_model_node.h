#ifndef PLAIN_RESOURCE_MODEL_NODE_H
#define PLAIN_RESOURCE_MODEL_NODE_H

#include <core/resource/resource_fwd.h>
#include <utils/common/uuid.h>

class QnPlainResourceModelNode {
public:
    explicit QnPlainResourceModelNode(const QnResourcePtr &resource);

    QnUuid id() const;
    QString name() const;
    int type() const;
    QnMediaServerResourcePtr server() const;

    QVariant data(int role) const;

    void update();

private:
    QnResourcePtr m_resource;

    QnUuid m_id;
    int m_type;
    QString m_name;
};

#endif // PLAIN_RESOURCE_MODEL_NODE_H
