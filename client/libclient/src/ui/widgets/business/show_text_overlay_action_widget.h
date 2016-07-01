#pragma once

#include <QtWidgets/QWidget>

#include <ui/widgets/business/abstract_business_params_widget.h>

namespace Ui {
    class ShowTextOverlayActionWidget;
}

// TODO: #ynikitenkov Refactor this and find common parts with bookmarks action widget.
class QnShowTextOverlayActionWidget : public QnAbstractBusinessParamsWidget
{
    Q_OBJECT
    typedef QnAbstractBusinessParamsWidget base_type;

public:
    explicit QnShowTextOverlayActionWidget(QWidget *parent = 0);
    ~QnShowTextOverlayActionWidget();

    virtual void updateTabOrder(QWidget *before, QWidget *after) override;

protected slots:
    virtual void at_model_dataChanged(QnBusiness::Fields fields) override;

private slots:
    void paramsChanged();

private:
    static QString getPlaceholderText();

private:
    QScopedPointer<Ui::ShowTextOverlayActionWidget> ui;

    QString m_lastCustomText;
};
