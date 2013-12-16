#ifndef ABSTRACT_PTZ_DIALOG_H
#define ABSTRACT_PTZ_DIALOG_H

#include <common/common_globals.h>

#include <core/ptz/ptz_fwd.h>

#include <ui/dialogs/button_box_dialog.h>

class QnAbstractPtzDialog : public QnButtonBoxDialog {
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;
public:
    QnAbstractPtzDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnAbstractPtzDialog();

    const QnPtzControllerPtr &ptzController() const;
    void setPtzController(const QnPtzControllerPtr &controller);

    virtual void accept() override;

protected:
    virtual void loadData(const QnPtzData &data) = 0;
    virtual void saveData() const = 0;
    virtual Qn::PtzDataFields requiredFields() const = 0;

signals:
    void synchronizeLater();

private slots:
    void synchronize();
    void at_controller_finished(Qn::PtzCommand command, const QVariant &data);

private:
    QnPtzControllerPtr m_controller;

};

#endif // ABSTRACT_PTZ_DIALOG_H
