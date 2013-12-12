#ifndef PTZ_TOURS_DIALOG_H
#define PTZ_TOURS_DIALOG_H

#include <QtWidgets/QDialog>

#include <core/ptz/ptz_fwd.h>

#include <ui/dialogs/button_box_dialog.h>

namespace Ui {
class QnPtzToursDialog;
}

class QnPtzTourListModel;

class QnPtzToursDialog : public QnButtonBoxDialog {
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;
public:
    explicit QnPtzToursDialog(QWidget *parent = 0);
    ~QnPtzToursDialog();

    const QnPtzControllerPtr &ptzController() const;
    void setPtzController(const QnPtzControllerPtr &controller);

    virtual void accept() override;

private:
    void updateModel();

private slots:
    void at_table_currentRowChanged(const QModelIndex &current, const QModelIndex &previous);
    void at_addTourButton_clicked();
    void at_deleteTourButton_clicked();

private:
    QScopedPointer<Ui::QnPtzToursDialog> ui;
    QnPtzControllerPtr m_controller;
    QnPtzTourListModel *m_model;
};

#endif // PTZ_TOURS_DIALOG_H
