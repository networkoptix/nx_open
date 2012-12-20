#ifndef RECORDING_BUSINESS_ACTION_WIDGET_H
#define RECORDING_BUSINESS_ACTION_WIDGET_H

#include <QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class QnRecordingBusinessActionWidget;
}

class QnRecordingBusinessActionWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;
    
public:
    explicit QnRecordingBusinessActionWidget(QWidget *parent = 0);
    ~QnRecordingBusinessActionWidget();
    
    virtual void loadParameters(const QnBusinessParams &params) override;
    virtual QnBusinessParams parameters() const override;
    virtual QString description() const override;
private:
    Ui::QnRecordingBusinessActionWidget *ui;
};

#endif // RECORDING_BUSINESS_ACTION_WIDGET_H
