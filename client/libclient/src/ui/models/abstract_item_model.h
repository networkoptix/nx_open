#pragma once

#include <QtCore/QAbstractItemModel>

class QnAbstractItemModel;

class QnAbstractItemModelResetter
{
public:
    QnAbstractItemModelResetter(QnAbstractItemModel *model);
    ~QnAbstractItemModelResetter();

private:
    QnAbstractItemModel* m_model;
};

/** Abstract model class containing some useful methods. */
class QnAbstractItemModel: public QAbstractItemModel {
public:
    explicit QnAbstractItemModel(QObject *parent = 0);
    virtual ~QnAbstractItemModel();
private:
    friend class QnAbstractItemModelResetter;
};

#define QN_SCOPED_MODEL_RESET(MODEL) QnAbstractItemModelResetter resetter(MODEL); Q_UNUSED(resetter);
