// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ping_dialog.h"

#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

#include <utils/ping_utility.h>

#ifdef Q_OS_MACOS
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
    connect(m_pingUtility,  &QnPingUtility::pingResponse,   this,   &QnPingDialog::at_pingUtility_pingResponse, Qt::QueuedConnection);
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

void QnPingDialog::at_pingUtility_pingResponse(const QnPingUtility::PingResponse &response) {
    m_pingText->append(responseToString(response));
}

QString QnPingDialog::responseToString(const QnPingUtility::PingResponse &response) const {
    switch (response.type) {
    case QnPingUtility::UnknownError:
        return QString(QString("Unknown error for icmp_seq %1")).arg(response.seq);
    case QnPingUtility::Timeout:
        return QString(QString("Request timeout for icmp_seq %1")).arg(response.seq);
#ifdef Q_OS_MACOS
    case ICMP_ECHOREPLY:
        return QString(QString("%1 bytes from %2: icmp_seq=%3 ttl=%4 time=%5 ms")).
                arg(response.bytes).
                arg(response.hostAddress).
                arg(response.seq).
                arg(response.ttl).
                arg(response.time, 0, 'f', 3);
    case ICMP_UNREACH:
        return QString(QString("Destination host unreacheable"));
#endif
    default:
        break;
    }
    return QString();
}
