#include "bookmark_business_action_widget.h"
#include "ui_bookmark_business_action_widget.h"

#include <business/business_action_parameters.h>

#include <utils/common/scoped_value_rollback.h>

QnBookmarkBusinessActionWidget::QnBookmarkBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::BookmarkBusinessActionWidget)
{
    ui->setupUi(this);

    connect(ui->tagsLineEdit, &QLineEdit::textChanged, this, &QnBookmarkBusinessActionWidget::paramsChanged);
}

QnBookmarkBusinessActionWidget::~QnBookmarkBusinessActionWidget()
{}

void QnBookmarkBusinessActionWidget::updateTabOrder( QWidget *before, QWidget *after ) {
    setTabOrder(before, ui->tagsLineEdit);
    setTabOrder(ui->tagsLineEdit, after);
}

void QnBookmarkBusinessActionWidget::at_model_dataChanged( QnBusinessRuleViewModel *model, QnBusiness::Fields fields ) {
    if (!model || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields & QnBusiness::ActionParamsField)
        ui->tagsLineEdit->setText(model->actionParams().tags);
}

void QnBookmarkBusinessActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    QnBusinessActionParameters params = model()->actionParams();
    params.tags = ui->tagsLineEdit->text();
    model()->setActionParams(params);
}
