#include "popup_widget.h"
#include "ui_popup_widget.h"

#include <utils/common/synctime.h>
#include <utils/settings.h>

#include <business/events/mserver_conflict_business_event.h>

QnPopupWidget::QnPopupWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QnPopupWidget),
    m_eventType(BusinessEventType::BE_NotDefined),
    m_eventCount(0)
{
    ui->setupUi(this);
    connect(ui->okButton, SIGNAL(clicked()), this, SLOT(at_okButton_clicked()));

    m_headerLabels << ui->warningLabel << ui->importantLabel << ui->notificationLabel;
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
        initAction(businessAction);

    int count = qMin(businessAction->getAggregationCount(), 1);
    m_eventCount += count;

    updateDetails(businessAction);

    if (m_eventCount == 1)
        showSingle();
    else
        showMultiple();
}

void QnPopupWidget::initAction(const QnAbstractBusinessActionPtr &businessAction) {
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
        case BusinessEventType::BE_Camera_Ip_Conflict:
        case BusinessEventType::BE_MediaServer_Failure:
        case BusinessEventType::BE_MediaServer_Conflict:
            ui->warningLabel->setVisible(true);
            break;

        //case BusinessEventType::BE_Camera_Motion:
        //case BusinessEventType::BE_UserDefined:
        default:
            ui->notificationLabel->setVisible(true);
            break;
    }
    m_eventTime = qnSyncTime->currentDateTime().toString(QLatin1String("hh:mm:ss"));
}

void QnPopupWidget::updateDetails(const QnAbstractBusinessActionPtr &businessAction) {
    BusinessEventType::Value eventType = QnBusinessEventRuntime::getEventType(businessAction->getRuntimeParams());

    switch (eventType) {
        case BusinessEventType::BE_Camera_Disconnect:
        case BusinessEventType::BE_Camera_Input:
        case BusinessEventType::BE_Camera_Motion:
            updateCameraDetails(businessAction);
            break;
        default:
            ui->detailsLabel->setVisible(false);
            break;
    }
}

void QnPopupWidget::updateCameraDetails(const QnAbstractBusinessActionPtr &businessAction) {
    //TODO: #GDM tr()
    static const QLatin1String resourceDetailsSummary("<html><head/><body><p>at %1</span></p></body></html>");
    static const QLatin1String resourceRepeat("%1 (%2 times)");
    static const QLatin1String cameraImg("<img src=\":/skin/tree/camera.png\" width=\"16\" height=\"16\"/>");

    QString resource = QnBusinessEventRuntime::getEventResourceName(businessAction->getRuntimeParams());
    if (qnSettings->isIpShownInTree())
        resource = QString(QLatin1String("%1 (%2)"))
                .arg(resource)
                .arg(QnBusinessEventRuntime::getEventResourceUrl(businessAction->getRuntimeParams()));

    int count = qMin(businessAction->getAggregationCount(), 1);

    if (!m_resourcesCount.contains(resource))
        m_resourcesCount[resource] = count;
    else
        m_resourcesCount[resource] += count;

    QString text = resourceDetailsSummary;
    if (m_eventCount == m_resourcesCount[resource]) { //single event occured some times
        text = text.arg(cameraImg + resource);
    } else {
        QString cameras;
        foreach (QString key, m_resourcesCount.keys()) {
            if (cameras.length() > 0)
                cameras += QLatin1String("<br>");
            cameras += cameraImg;
            int n = m_resourcesCount[key];
            if (n > 1)
                cameras += QString(resourceRepeat).arg(key).arg(n);
            else
                cameras += resource;
        }
        text = text.arg(cameras);
    }

    ui->detailsLabel->setText(text);
}

void QnPopupWidget::updateConflictECDetails(const QnAbstractBusinessActionPtr &businessAction) {

    //TODO: #GDM tr()
    static const QLatin1String resourceDetailsSummary("<html><head/><body><p>EC %1 <br>conflicting with <br>%2</span></p></body></html>");

    QString current = QnBusinessEventRuntime::getCurrentEC(businessAction->getRuntimeParams());
    QString conflicting = QnBusinessEventRuntime::getConflictECs(businessAction->getRuntimeParams()).join(QLatin1String("<br>"));

    ui->detailsLabel->setText(QString(resourceDetailsSummary).arg(current).arg(conflicting));

}

void QnPopupWidget::showSingle() {

    //TODO: #GDM tr()
    static const QLatin1String singleEvent("<html><head/><body><p>%1 at <span style=\" text-decoration: underline;\">%3</span></p></body></html>");

    QString eventString = BusinessEventType::toString(m_eventType);
    QString description = QString(singleEvent).arg(eventString).arg(m_eventTime);
    ui->eventLabel->setText(description);
}

void QnPopupWidget::showMultiple() {
    //TODO: #GDM tr()
    static const QLatin1String multipleEvents("<html><head/><body><p>%1 <span style=\" font-weight:600;\">%2 times </span>"\
                                        "since <span style=\" text-decoration: underline;\">%3</span></p></body></html>");


    QString eventString = BusinessEventType::toString(m_eventType);
    QString description = QString(multipleEvents).arg(eventString).arg(m_eventCount).arg(m_eventTime);
    ui->eventLabel->setText(description);
}
