#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>

#include <nx/utils/impl_ptr.h>

namespace Ui { class CameraListDialog; }

class QnCameraListDialog: public QnSessionAwareDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareDialog;

public:
    explicit QnCameraListDialog(QWidget* parent);
    virtual ~QnCameraListDialog() override;

    void setServer(const QnMediaServerResourcePtr& server);
    QnMediaServerResourcePtr server() const;

private:
    void updateWindowTitle();
    void updateCriterion();

    void at_camerasView_customContextMenuRequested(const QPoint& pos);
    void at_exportAction_triggered();
    void at_clipboardAction_triggered();
    void at_camerasView_doubleClicked(const QModelIndex& index);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
    nx::utils::ImplPtr<Ui::CameraListDialog> ui;
};
