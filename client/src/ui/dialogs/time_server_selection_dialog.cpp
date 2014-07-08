/**********************************************************
* 08 jul 2014
* a.kolesnikov
***********************************************************/

#include "time_server_selection_dialog.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include "ui_time_server_selection_dialog.h"


static const int checkBoxColumn = 0;
static const int serverNameColumn = 1;
static const int serverTimeColumn = 2;

QnTimeServerSelectionDialog::QnTimeServerSelectionDialog( QWidget* parent, QnWorkbenchContext* context )
:
    QDialog( parent ),
    QnWorkbenchContextAware( parent, context ),
    m_ui( new Ui::TimeServerSelectionDialog )
{
    m_ui->setupUi(this);

    setHelpTopic(this, Qn::PrimaryTimeServerSelection_Help);
    //m_header->setSectionResizeMode(checkBoxColumn, QHeaderView::ResizeToContents);
    //m_header->setSectionResizeMode(serverNameColumn, QHeaderView::ResizeToContents);
    //m_header->setSectionResizeMode(serverTimeColumn, QHeaderView::ResizeToContents);
    //m_header->setSectionsClickable(true);
}

QnTimeServerSelectionDialog::~QnTimeServerSelectionDialog()
{
}

/*!
    \param peersAndTimes list<peer id; peer local time (UTC, millis from epoch) corresponding to \a localSystemTime>
*/
void QnTimeServerSelectionDialog::setData(
    qint64 localSystemTime,
    const QList<QPair<QnId, qint64> >& peersAndTimes )
{
    m_ui->serversWidget->setRowCount(0);

    for( const QPair<QnId, qint64>& peer: peersAndTimes )
    {
        const QnResourcePtr& res = QnResourcePool::instance()->getResourceById( peer.first );
        if( !res )
            continue;
        const QnMediaServerResource* mediaServerRes = dynamic_cast<const QnMediaServerResource*>(res.data());
        if( !mediaServerRes )
            continue;

        const int row = m_ui->serversWidget->rowCount();
        m_ui->serversWidget->insertRow( row );

        QTableWidgetItem* checkItem = new QTableWidgetItem();
        checkItem->setFlags(checkItem->flags() | Qt::ItemIsUserCheckable);
        //checkItem->setFlags(checkItem->flags() | Qt::ItemIsTristate);
        checkItem->setFlags(checkItem->flags() &~ Qt::ItemIsEnabled);
        checkItem->setCheckState(Qt::Unchecked);
        //checkItem->setData(Qt::UserRole, qVariantFromValue<QnManualCameraSearchSingleCamera>(info));

        QTableWidgetItem* serverNameItem = new QTableWidgetItem( mediaServerRes->getName() );
        serverNameItem->setFlags(serverNameItem->flags() & ~Qt::ItemIsEditable);

        QTableWidgetItem* serverTimeItem = new QTableWidgetItem( QDateTime::fromMSecsSinceEpoch(peer.second).toString() );
        serverTimeItem->setFlags(serverTimeItem->flags() & ~Qt::ItemIsEditable);

        m_ui->serversWidget->setItem(row, checkBoxColumn, checkItem);
        m_ui->serversWidget->setItem(row, serverNameColumn, serverNameItem);
        m_ui->serversWidget->setItem(row, serverTimeColumn, serverTimeItem);
    }
}

void QnTimeServerSelectionDialog::accept()
{
    //TODO

    QDialog::accept();
}
