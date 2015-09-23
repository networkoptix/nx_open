#include "abstract_item_model.h"

QnAbstractItemModelResetter::QnAbstractItemModelResetter(QnAbstractItemModel *model) :
    m_model(model)
{
    m_model->beginResetModel();
}

QnAbstractItemModelResetter::~QnAbstractItemModelResetter() {
    m_model->endResetModel();
}

QnAbstractItemModel::QnAbstractItemModel(QObject *parent /* = 0*/):
    QAbstractItemModel(parent)
{}

QnAbstractItemModel::~QnAbstractItemModel()
{}
