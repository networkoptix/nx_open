#pragma once

#include <QtCore/QAbstractListModel>

#include <core/resource/resource_fwd.h>

#include <nx/utils/scoped_model_operations.h>

class QnLayoutTourItemsModel: public ScopedModelOperations<QAbstractListModel>
{
    using base_type = ScopedModelOperations<QAbstractListModel>;
    Q_OBJECT
public:
    enum Column
    {
        OrderColumn,
        LayoutColumn,
        DelayColumn,
        MoveUpColumn, //TODO: #GDM #3.1 remove if will support the dialog
        MoveDownColumn,
        ControlsColumn,

        ColumnCount
    };

    QnLayoutTourItemsModel(QObject* parent = nullptr);
    virtual ~QnLayoutTourItemsModel() override;

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void reset(const QnLayoutTourItemList& items);
    void addItem(const QnLayoutTourItem& item);
    void moveUp(int row);
    void moveDown(int row);

    QnLayoutTourItemList items() const;

private:
    QnLayoutTourItemList m_items;
};
