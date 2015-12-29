#pragma once

#include <QtWidgets/QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class BookmarkBusinessActionWidget;
}

class QnBookmarkBusinessActionWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit QnBookmarkBusinessActionWidget(QWidget *parent = 0);
    ~QnBookmarkBusinessActionWidget();

    virtual void updateTabOrder(QWidget *before, QWidget *after) override;

protected slots:
    virtual void at_model_dataChanged(QnBusiness::Fields fields) override;

private slots:
    void paramsChanged();

private:
    QScopedPointer<Ui::BookmarkBusinessActionWidget> ui;
};
