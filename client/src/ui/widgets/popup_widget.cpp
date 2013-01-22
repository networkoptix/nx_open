#include "popup_widget.h"
#include "ui_popup_widget.h"

#include <utils/common/synctime.h>

namespace {

    static QLatin1String cameraResource("Camera <img src=\":/skin/tree/camera_offline.png\" width=\"16\" height=\"16\"/> %1");

    static QLatin1String singleEvent("<html><head/><body><p>%1 at <span style=\" text-decoration: underline;\">%3</span></p></body></html>");

    static QLatin1String multipleEvents("<html><head/><body><p>%1 <span style=\" font-weight:600;\">%2 times </span>"\
                                        "since <span style=\" text-decoration: underline;\">%3</span></p></body></html>");


}

QnPopupWidget::QnPopupWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnPopupWidget),
    m_eventType(BusinessEventType::BE_NotDefined),
    m_eventCount(0)
{
    ui->setupUi(this);
    connect(ui->okButton, SIGNAL(clicked()), this, SLOT(at_okButton_clicked()));

    m_headerLabels << ui->errorLabel << ui->warningLabel << ui->importantLabel << ui->notificationLabel;
}

QnPopupWidget::~QnPopupWidget()
{
    delete ui;
}

void QnPopupWidget::at_okButton_clicked() {
    bool ignore = ui->checkBox->isChecked();
    emit closed(m_eventType, ignore);
}

void QnPopupWidget::addBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    if (m_eventCount == 0) //not initialized
        showSingleAction(businessAction);
    else
        showMultipleActions(businessAction);
}

void QnPopupWidget::showSingleAction(const QnAbstractBusinessActionPtr &businessAction) {
    foreach (QWidget* w, m_headerLabels)
        w->setVisible(false);

    m_eventType = QnBusinessEventRuntime::getEventType(businessAction->getRuntimeParams());
    switch (m_eventType) {
        case BusinessEventType::BE_NotDefined:
            return;

        case BusinessEventType::BE_Camera_Input:
        case BusinessEventType::BE_Camera_Disconnect:
            ui->importantLabel->setVisible(true);
            break;

        case BusinessEventType::BE_Storage_Failure:
        case BusinessEventType::BE_Network_Issue:
            ui->warningLabel->setVisible(true);
            break;

        case BusinessEventType::BE_Camera_Ip_Conflict:
        case BusinessEventType::BE_MediaServer_Failure:
        case BusinessEventType::BE_MediaServer_Conflict:
            ui->errorLabel->setVisible(true);
            break;

        //case BusinessEventType::BE_Camera_Motion:
        //case BusinessEventType::BE_UserDefined:
        default:
            ui->notificationLabel->setVisible(true);
            break;
    }

    m_eventCount = 1;
    m_eventTime = qnSyncTime->currentDateTime().toString(QLatin1String("hh:mm:ss"));

    QString eventString = BusinessEventType::toString(m_eventType);
//    QString eventResource = QString(cameraResource).arg(tr("Test camera"));

    QString description = QString(singleEvent).arg(eventString).arg(m_eventTime);
    ui->eventLabel->setText(description);
}

void QnPopupWidget::showMultipleActions(const QnAbstractBusinessActionPtr &businessAction) {
    Q_UNUSED(businessAction)

    m_eventCount++;
    QString eventString = BusinessEventType::toString(m_eventType);
    QString description = QString(multipleEvents).arg(eventString).arg(m_eventCount).arg(m_eventTime);
    ui->eventLabel->setText(description);
}
