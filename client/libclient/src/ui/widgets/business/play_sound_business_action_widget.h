#ifndef PLAY_SOUND_BUSINESS_ACTION_WIDGET_H
#define PLAY_SOUND_BUSINESS_ACTION_WIDGET_H

#include <QtWidgets/QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class PlaySoundBusinessActionWidget;
}

class QnPlaySoundBusinessActionWidget : public QnAbstractBusinessParamsWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit QnPlaySoundBusinessActionWidget(QWidget *parent = 0);
    ~QnPlaySoundBusinessActionWidget();

    virtual void updateTabOrder(QWidget *before, QWidget *after) override;

protected slots:
    virtual void at_model_dataChanged(QnBusiness::Fields fields) override;

private slots:
    void paramsChanged();
    void updateCurrentIndex();
    void enableTestButton();

    void at_testButton_clicked();
    void at_manageButton_clicked();
    void at_soundModel_itemChanged(const QString &filename);
    void at_volumeSlider_valueChanged(int value);

private:
    QScopedPointer<Ui::PlaySoundBusinessActionWidget> ui;

    QString m_filename;
};

#endif // PLAY_SOUND_BUSINESS_ACTION_WIDGET_H
