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
    void updateCurrentIndex();

    void at_playButton_clicked();
    void at_manageButton_clicked();
    void at_soundModel_itemChanged(const QString &filename);
private:
    QScopedPointer<Ui::QnPlaySoundBusinessActionWidget> ui;

    QString m_filename;
};

#endif // PLAY_SOUND_BUSINESS_ACTION_WIDGET_H
