#pragma once

#include <QtCore/QSortFilterProxyModel>

class QnLayoutsModel : public QSortFilterProxyModel
{
    Q_OBJECT

    using base_type = QSortFilterProxyModel;

public:
    enum class ItemType
    {
        AllCameras,
        Layout,
        LiteClient
    };
    Q_ENUM(ItemType)

    QnLayoutsModel(QObject* parent = nullptr);

    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};
