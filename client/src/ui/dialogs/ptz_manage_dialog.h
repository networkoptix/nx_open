#ifndef PTZ_TOURS_DIALOG_H
#define PTZ_TOURS_DIALOG_H

#include <core/resource/resource_fwd.h>
#include <core/ptz/ptz_fwd.h>

#include <ui/dialogs/abstract_ptz_dialog.h>
#include <utils/common/singleton.h>

namespace Ui {
    class PtzManageDialog;
}

class QnPtzManageModel;

// TODO: #GDM remove singleton
class QnPtzManageDialog : public QnAbstractPtzDialog, public Singleton<QnPtzManageDialog> {
    Q_OBJECT
    typedef QnAbstractPtzDialog base_type;

public:
    explicit QnPtzManageDialog(QWidget *parent = 0);
    ~QnPtzManageDialog();

    QnResourcePtr resource() const;
    void setResource(const QnResourcePtr &resource);

protected:
    virtual void loadData(const QnPtzData &data) override;
    virtual void saveData() override;
    virtual Qn::PtzDataFields requiredFields() const override;

    virtual void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateUi();

    void at_tableView_currentRowChanged(const QModelIndex &current, const QModelIndex &previous);

    void at_savePositionButton_clicked();
    void at_goToPositionButton_clicked();
    void at_addTourButton_clicked();
    void at_startTourButton_clicked();
    void at_deleteButton_clicked();

    void at_tableViewport_resizeEvent();
    void at_tourSpotsChanged(const QnPtzTourSpotList &spots);
private:
    bool savePresets();
    bool saveTours();

    QScopedPointer<Ui::PtzManageDialog> ui;
    QnPtzManageModel *m_model;
    QnResourcePtr m_resource;

    QString m_currentTourId;
};

#endif // PTZ_TOURS_DIALOG_H
