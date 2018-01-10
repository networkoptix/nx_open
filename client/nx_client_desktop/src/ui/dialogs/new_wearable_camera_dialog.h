#pragma once

#include <ui/dialogs/common/session_aware_dialog.h>
#include <core/resource/resource_fwd.h>

namespace Ui { class NewWearableCameraDialog; }

class QnResourceListModel;

class QnNewWearableCameraDialog : public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit QnNewWearableCameraDialog(QWidget* parent);
    ~QnNewWearableCameraDialog();

    QString name() const;
    const QnMediaServerResourcePtr server() const;

private:
    QScopedPointer<Ui::NewWearableCameraDialog> ui;
    QnResourceListModel* m_model;
};
