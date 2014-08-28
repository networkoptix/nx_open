#include "time_server_selection_widget.h"
#include "ui_time_server_selection_widget.h"

#include <ui/models/time_server_selection_model.h>

QnTimeServerSelectionWidget::QnTimeServerSelectionWidget(QWidget *parent /*= NULL*/):
    QnAbstractPreferencesWidget(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::TimeServerSelectionWidget),
    m_model(new QnTimeServerSelectionModel(this))
{
    ui->setupUi(this);

    ui->serversTable->setModel(m_model);

    ui->serversTable->horizontalHeader()->setSectionResizeMode(QnTimeServerSelectionModel::CheckboxColumn, QHeaderView::ResizeToContents);
    ui->serversTable->horizontalHeader()->setSectionResizeMode(QnTimeServerSelectionModel::NameColumn, QHeaderView::Stretch);
    ui->serversTable->horizontalHeader()->setSectionResizeMode(QnTimeServerSelectionModel::TimeColumn, QHeaderView::ResizeToContents);
    ui->serversTable->horizontalHeader()->setSectionsClickable(false);

    QTimer* timer = new QTimer(this);
    timer->setInterval(1000);
    timer->setSingleShot(false);
    connect(timer, &QTimer::timeout, this, [this]{
        if (!isVisible())
            return;
        m_model->updateTime();
    });
}


QnTimeServerSelectionWidget::~QnTimeServerSelectionWidget()
{

}


void QnTimeServerSelectionWidget::updateFromSettings()
{

}

void QnTimeServerSelectionWidget::submitToSettings()
{

}

bool QnTimeServerSelectionWidget::hasChanges() const 
{
    return false;
}
