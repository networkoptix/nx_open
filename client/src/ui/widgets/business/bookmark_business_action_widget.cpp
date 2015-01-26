#include "bookmark_business_action_widget.h"
#include "ui_bookmark_business_action_widget.h"

QnBookmarkBusinessActionWidget::QnBookmarkBusinessActionWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::BookmarkBusinessActionWidget)
{
    ui->setupUi(this);
}

QnBookmarkBusinessActionWidget::~QnBookmarkBusinessActionWidget()
{
}
