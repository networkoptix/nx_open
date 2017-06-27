#pragma once

#include <QtCore/QScopedPointer>

#include <ui/widgets/business/abstract_business_params_widget.h>

class QnBusinessStringsHelper;

class QnSubjectTargetActionWidget: public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    using base_type = QnAbstractBusinessParamsWidget;

public:
    explicit QnSubjectTargetActionWidget(QWidget* parent = nullptr);
    virtual ~QnSubjectTargetActionWidget() override;

protected:
    virtual void at_model_dataChanged(QnBusiness::Fields fields) override;

    //TODO: #vkutin #3.2 Refactor this along with all event/action widgets.
    void setSubjectsButton(QPushButton* button);

private:
    void updateSubjectsButton();
    void selectSubjects();

private:
    QPushButton* m_subjectsButton = nullptr;
    QScopedPointer<QnBusinessStringsHelper> m_helper;
};
