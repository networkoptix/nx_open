#ifndef PTZ_TOURS_DIALOG_H
#define PTZ_TOURS_DIALOG_H

#include <core/ptz/ptz_fwd.h>

#include <ui/dialogs/abstract_ptz_dialog.h>

namespace Ui {
    class PtzToursDialog;
}

class QnPtzTourListModel;

class QnPtzToursDialog : public QnAbstractPtzDialog {
    Q_OBJECT

    typedef QnAbstractPtzDialog base_type;
public:
    explicit QnPtzToursDialog(const QnPtzControllerPtr &controller, QWidget *parent = 0);
    ~QnPtzToursDialog();

protected:
    virtual void loadData(const QnPtzData &data) override;
    virtual void saveData() override;
    virtual Qn::PtzDataFields requiredFields() const override;

private slots:
    void at_table_currentRowChanged(const QModelIndex &current, const QModelIndex &previous);
    void at_addTourButton_clicked();
    void at_deleteTourButton_clicked();

private:
    bool saveTours();

    QScopedPointer<Ui::PtzToursDialog> ui;
    QnPtzTourListModel *m_model;
    QnPtzTourList m_oldTours;
};

#endif // PTZ_TOURS_DIALOG_H
