/**********************************************************
* 08 jul 2014
* a.kolesnikov
***********************************************************/

#include "time_server_selection_dialog.h"

#include <api/app_server_connection.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx_ec/dummy_handler.h>

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
    m_ui( new Ui::TimeServerSelectionDialog ),
    m_timeBase( 0 )
{
    m_ui->setupUi(this);

    connect( m_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept );

    setHelpTopic(this, Qn::PrimaryTimeServerSelection_Help);
    m_ui->serversWidget->horizontalHeader()->setSectionResizeMode(checkBoxColumn, QHeaderView::ResizeToContents);
    m_ui->serversWidget->horizontalHeader()->setSectionResizeMode(serverNameColumn, QHeaderView::Stretch);
    m_ui->serversWidget->horizontalHeader()->setSectionResizeMode(serverTimeColumn, QHeaderView::ResizeToContents);
    m_ui->serversWidget->horizontalHeader()->setSectionsClickable(true);

    connect( m_ui->serversWidget, &QTableWidget::itemClicked, this, &QnTimeServerSelectionDialog::onTableItemClicked );
    connect( &m_timer, &QTimer::timeout, this, &QnTimeServerSelectionDialog::onTimer );
}

QnTimeServerSelectionDialog::~QnTimeServerSelectionDialog()
{
}

/*!
    \param peersAndTimes list<peer id; peer local time (UTC, millis from epoch) corresponding to \a localSystemTime>
*/
void QnTimeServerSelectionDialog::setData(
    qint64 localSystemTime,
    const QList<QPair<QUuid, qint64> >& peersAndTimes )
{
    const qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    m_timeBase = localSystemTime;
    m_ui->serversWidget->setRowCount(0);

    m_peersAndTimes = peersAndTimes;

    for( int i = 0; i < peersAndTimes.size(); ++i )
    {
        const auto& peer = peersAndTimes[i];

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
        checkItem->setFlags(checkItem->flags() &~ Qt::ItemIsEnabled);
        checkItem->setCheckState( row == 0 ? Qt::Checked : Qt::Unchecked );
        checkItem->setData(Qt::UserRole, i);

        QTableWidgetItem* serverNameItem = new QTableWidgetItem( lit("%1 (%2)").arg(mediaServerRes->getName()).arg(QUrl(mediaServerRes->getUrl()).host()) );
        serverNameItem->setFlags(serverNameItem->flags() & ~Qt::ItemIsEditable);

        QTableWidgetItem* serverTimeItem = new QTableWidgetItem( QDateTime::fromMSecsSinceEpoch(currentTime - m_timeBase + peer.second).toString(Qt::ISODate) );
        serverTimeItem->setFlags(serverTimeItem->flags() & ~Qt::ItemIsEditable);

        m_ui->serversWidget->setItem(row, checkBoxColumn, checkItem);
        m_ui->serversWidget->setItem(row, serverNameColumn, serverNameItem);
        m_ui->serversWidget->setItem(row, serverTimeColumn, serverTimeItem);
    }
}

QUuid QnTimeServerSelectionDialog::selectedPeer() const
{
    for( int row = 0; row < m_ui->serversWidget->rowCount(); ++row )
    {
        if( m_ui->serversWidget->item(row, checkBoxColumn)->checkState() == Qt::Checked )
            return m_peersAndTimes[m_ui->serversWidget->item(row, checkBoxColumn)->data(Qt::UserRole).toInt()].first;
    }

    return QUuid();
}

void QnTimeServerSelectionDialog::accept()
{
    QDialog::accept();

    const QUuid& selectedPeerID = selectedPeer();
    if( selectedPeerID.isNull() )
        return;
    const ec2::AbstractECConnectionPtr& connection = QnAppServerConnectionFactory::getConnection2();
    if( !connection )
        return;
    connection->getTimeManager()->forcePrimaryTimeServer(
        selectedPeerID,
        ec2::DummyHandler::instance(),
        &ec2::DummyHandler::onRequestDone );
}

void QnTimeServerSelectionDialog::showEvent( QShowEvent* )
{
    m_timer.start( 300 );
}

void QnTimeServerSelectionDialog::hideEvent( QHideEvent* )
{
    m_timer.stop();
}

void QnTimeServerSelectionDialog::onTimer()
{
    const qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    //updating peer times
    for( int i = 0; i < m_ui->serversWidget->rowCount(); ++i )
    {
        QTableWidgetItem* checkItem = m_ui->serversWidget->item( i, checkBoxColumn );
        QTableWidgetItem* serverTimeItem = m_ui->serversWidget->item( i, serverTimeColumn );
        assert( checkItem );
        const auto& peer = m_peersAndTimes[checkItem->data(Qt::UserRole).toInt()];
        serverTimeItem->setText( QDateTime::fromMSecsSinceEpoch(currentTime - m_timeBase + peer.second).toString(Qt::ISODate) );
    }
}

void QnTimeServerSelectionDialog::onTableItemClicked( QTableWidgetItem* item )
{
    if( item->column() != checkBoxColumn )
        return;

    item->setCheckState( Qt::Checked );
    for( int row = 0; row < m_ui->serversWidget->rowCount(); ++row )
    {
        if( row == item->row() )
            continue;
        m_ui->serversWidget->item(row, item->column())->setCheckState( Qt::Unchecked );
    }
}
