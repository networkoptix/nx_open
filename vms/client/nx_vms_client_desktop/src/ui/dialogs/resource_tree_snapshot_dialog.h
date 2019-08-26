#pragma once

#include <ui/dialogs/common/session_aware_dialog.h>
#include <QtCore/QModelIndex>

namespace Ui { class ResourceTreeSnapshotDialog; }
class QAbstractItemModel;

namespace nx::vms::client::desktop {

class ResourceTreeSnapshotDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit ResourceTreeSnapshotDialog(QAbstractItemModel* treeModel, QWidget* parent);
    virtual ~ResourceTreeSnapshotDialog() override;

private:
    void updateShapshot();
    QModelIndex indexFromString(const QString& string) const;

private:
    QScopedPointer<Ui::ResourceTreeSnapshotDialog> ui;
    QAbstractItemModel* m_treeModel;
};

} // namespace nx::vms::client::desktop
