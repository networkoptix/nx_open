#pragma once

#include <QtWidgets/QWidget>
#include <ui/widgets/business/abstract_business_params_widget.h>
#include "core/ptz/ptz_fwd.h"

namespace Ui {
    class ExecPtzPresetBusinessActionWidget;
}

class QnExecPtzPresetBusinessActionWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit QnExecPtzPresetBusinessActionWidget(QWidget *parent = 0);
    ~QnExecPtzPresetBusinessActionWidget();

    virtual void updateTabOrder(QWidget *before, QWidget *after) override;

protected slots:
    virtual void at_model_dataChanged(Fields fields) override;
private slots:
    void paramsChanged();
    void updateComboboxItems(const QString& presetId);
private:
    void setupPtzController(const QnVirtualCameraResourcePtr& camera);
private:
    QScopedPointer<Ui::ExecPtzPresetBusinessActionWidget> ui;
    QnPtzControllerPtr m_ptzController;
    QnPtzPresetList m_presets;
};
