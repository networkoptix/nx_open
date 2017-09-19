#pragma once

#include <QtCore/QAbstractItemModel>

#include <utils/common/connective.h>

namespace nx {
namespace client {

class ModelDataAccessor: public Connective<QObject>
{
    using base_type = Connective<QObject>;

    Q_OBJECT
    Q_PROPERTY(QVariant model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    ModelDataAccessor(QObject* parent = nullptr);
    ~ModelDataAccessor();

    QVariant model() const;
    void setModel(const QVariant& modelVariant);

    int count() const;

    Q_INVOKABLE QVariant getData(int row, const QString& roleName) const;

signals:
    void rowsMoved();
    void modelChanged();
    void countChanged();
    void dataChanged(int startRow, int endRow);

private:
    QPointer<QAbstractItemModel> m_model;
};

} // namespace client
} // namespace nx
