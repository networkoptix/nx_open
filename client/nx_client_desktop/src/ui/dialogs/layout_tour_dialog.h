#pragma once

#include <QtCore/QModelIndex>

#include <ui/dialogs/common/session_aware_dialog.h>

#include <nx_ec/data/api_fwd.h>

namespace Ui {
class LayoutTourDialog;
} // namespace Ui

class QnLayoutTourItemsModel;

class QnLayoutTourDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    typedef QnSessionAwareButtonBoxDialog base_type;

public:
    QnLayoutTourDialog(QWidget* parent = nullptr, Qt::WindowFlags windowFlags = 0);
    virtual ~QnLayoutTourDialog();

    void loadData(const ec2::ApiLayoutTourData& tour);
    void submitData(ec2::ApiLayoutTourData* tour) const;

private:
    void at_view_clicked(const QModelIndex& index);

private:
    QScopedPointer<Ui::LayoutTourDialog> ui;
    QnLayoutTourItemsModel* m_model;
};
