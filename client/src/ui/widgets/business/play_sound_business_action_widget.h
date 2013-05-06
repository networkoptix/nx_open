#ifndef PLAY_SOUND_BUSINESS_ACTION_WIDGET_H
#define PLAY_SOUND_BUSINESS_ACTION_WIDGET_H

#include <QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnPlaySoundBusinessActionWidget;
}

class QnAppServerNotificationCache;

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
    void at_playButton_clicked();
    void at_manageButton_clicked();
    void at_fileListReceived(const QStringList &filenames, bool ok);

private:
    void updateSoundUrl();

    QScopedPointer<Ui::QnPlaySoundBusinessActionWidget> ui;

    QnAppServerNotificationCache *m_cache;
    bool m_loaded;

    QString m_soundUrl;
};

#endif // PLAY_SOUND_BUSINESS_ACTION_WIDGET_H
