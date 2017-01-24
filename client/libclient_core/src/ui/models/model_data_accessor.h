#pragma once

#include <QtCore/QPointer>
#include <QtCore/QAbstractItemModel>

namespace nx {
namespace client {

class ModelDataAccessor: public QObject
{
    using base_type = QObject;

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
    void modelChanged();
    void countChanged();
    void dataChanged(int startRow, int endRow);

private:
    QPointer<QAbstractItemModel> m_model;
};

} // namespace client
} // namespace nx
