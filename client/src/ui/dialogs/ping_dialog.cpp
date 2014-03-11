#include "ping_dialog.h"

#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

#include <utils/network/icmp_mac.h>
#include <utils/ping_utility.h>


QnPingDialog::QnPingDialog(QWidget *parent) :
    base_type(parent, Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint
#ifdef Q_OS_MAC
    | Qt::Tool
#endif
    )
{
    m_pingText = new QTextEdit(this);
    m_pingText->setReadOnly(true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_pingText);

    m_pingUtility = new QnPingUtility(this);
    connect(m_pingUtility,  &QnPingUtility::pingResponce,   this,   &QnPingDialog::at_pingUtility_pingResponce, Qt::QueuedConnection);
    connect(m_pingUtility,  &QThread::finished,             this,   &QnPingDialog::deleteLater);

    setMinimumSize(300, 300);
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
    return lit("ping...");
}
