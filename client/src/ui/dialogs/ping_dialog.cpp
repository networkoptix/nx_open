#include "ping_dialog.h"

#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

#include <utils/ping_utility.h>

#ifdef Q_OS_MACX
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#endif

QnPingDialog::QnPingDialog(QWidget *parent, Qt::WindowFlags flags) :
    base_type(parent, flags)
{
    m_pingText = new QTextEdit(this);
    m_pingText->setReadOnly(true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_pingText);

    m_pingUtility = new QnPingUtility(this);
    connect(m_pingUtility,  &QnPingUtility::pingResponce,   this,   &QnPingDialog::at_pingUtility_pingResponce, Qt::QueuedConnection);
    connect(m_pingUtility,  &QThread::finished,             this,   &QnPingDialog::deleteLater);

    setMinimumSize(300, 300);
    resize(460, 300);
}

void QnPingDialog::startPings() {
    m_pingUtility->start();
}

void QnPingDialog::setHostAddress(const QString &hostAddress) {
    m_pingUtility->setHostAddress(hostAddress);
}

void QnPingDialog::closeEvent(QCloseEvent *event) {
    m_pingUtility->pleaseStop();
    base_type::closeEvent(event);
}

void QnPingDialog::at_pingUtility_pingResponce(const QnPingUtility::PingResponce &responce) {
    m_pingText->append(responceToString(responce));
}

QString QnPingDialog::responceToString(const QnPingUtility::PingResponce &responce) const {
    // TODO: #TR #dklychkov maybe add more details and translate the strings below
    switch (responce.type) {
    case QnPingUtility::UnknownError:
        return QString(lit("Unknown error for icmp_seq %1")).arg(responce.seq);
    case QnPingUtility::Timeout:
        return QString(lit("Request timeout for icmp_seq %1")).arg(responce.seq);
#ifdef Q_OS_MACX
    case ICMP_ECHOREPLY:
        return QString(lit("%1 bytes from %2: icmp_seq=%3 ttl=%4 time=%5 ms")).
                arg(responce.bytes).
                arg(responce.hostAddress).
                arg(responce.seq).
                arg(responce.ttl).
                arg(responce.time, 0, 'f', 3);
    case ICMP_UNREACH:
        return QString(lit("Destination host unreacheable"));
#endif
    default:
        break;
    }
    return QString();
}
