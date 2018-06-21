#pragma once

#include <QtCore/QAbstractItemModel>

#include <nx/utils/scoped_model_operations.h>

namespace nx {
namespace client {
namespace desktop {

class LayoutSelectionDialogState;

class LayoutsModel: public ScopedModelOperations<QAbstractItemModel>
{
    Q_OBJECT
    using base_type = ScopedModelOperations<QAbstractItemModel>;
    using State = LayoutSelectionDialogState;

public:
    LayoutsModel(QObject* parent = nullptr);
    virtual ~LayoutsModel() override;

    void loadState(const State& state);

    enum Columns
    {
        Name,
        Check,
        Count
    };

public: // Overrides section.
    virtual QModelIndex index(
        int row,
        int column,
        const QModelIndex& parent = QModelIndex()) const override;

    virtual QModelIndex parent(const QModelIndex& child) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

private:
    struct Private;
    const QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx

