#pragma once

#include <QtCore/QModelIndex>

#include <QtWidgets/QDialog>

#include <core/resource/resource_fwd.h>

#include <client/client_globals.h>

#include <ui/delegates/resource_selection_dialog_delegate.h>
#include <ui/workbench/workbench_context_aware.h>
#include "utils/common/id.h"

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui {
class ResourceSelectionDialog;
}

class QnResourceTreeModel;

class QnResourceSelectionDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT

    using base_type = QnSessionAwareButtonBoxDialog;
public:
    enum class Filter
    {
        cameras,
        users
    };

    explicit QnResourceSelectionDialog(Filter filter, QWidget* parent);
    ~QnResourceSelectionDialog();

    QSet<QnUuid> selectedResources() const;
    void setSelectedResources(const QSet<QnUuid> &selected);

    QnResourceSelectionDialogDelegate* delegate() const;
    void setDelegate(QnResourceSelectionDialogDelegate* delegate);

protected:
    virtual void keyPressEvent(QKeyEvent *event) override;

    QSet<QnUuid> selectedResourcesInternal(const QModelIndex& parent = QModelIndex()) const;
    void setSelectedResourcesInternal(const QSet<QnUuid>& selected,
        const QModelIndex& parent = QModelIndex());

private:
    void at_resourceModel_dataChanged();

    QModelIndex itemIndexAt(const QPoint &pos) const;
    void updateThumbnail(const QModelIndex &index);
    void initModel();

private:
    QScopedPointer<Ui::ResourceSelectionDialog> ui;
    QnResourceTreeModel* m_resourceModel;
    QnResourceSelectionDialogDelegate* m_delegate;
    Filter m_filter;

    bool m_updating;
};
