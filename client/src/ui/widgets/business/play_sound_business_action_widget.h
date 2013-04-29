#ifndef PLAY_SOUND_BUSINESS_ACTION_WIDGET_H
#define PLAY_SOUND_BUSINESS_ACTION_WIDGET_H

#include <QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnPlaySoundBusinessActionWidget;
}

class QnPlaySoundBusinessActionWidget : public QnAbstractBusinessParamsWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;
    
public:
    explicit QnPlaySoundBusinessActionWidget(QWidget *parent = 0);
    ~QnPlaySoundBusinessActionWidget();
    
protected slots:
    virtual void at_model_dataChanged(QnBusinessRuleViewModel *model, QnBusiness::Fields fields) override;

private slots:
    void paramsChanged();
    void at_manageButton_clicked();

private:
    QScopedPointer<Ui::QnPlaySoundBusinessActionWidget> ui;
};

#endif // PLAY_SOUND_BUSINESS_ACTION_WIDGET_H
