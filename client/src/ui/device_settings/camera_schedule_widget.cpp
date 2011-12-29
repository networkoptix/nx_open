#include "camera_schedule_widget.h"
#include "ui_camera_schedule.h"

CameraScheduleWidget::CameraScheduleWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CameraSchedule)
{
    ui->setupUi(this);
    connect(ui->chkBoxDisplayQuality, SIGNAL(stateChanged(int)), this, SLOT(onDisplayQualityChanged(int)));
    connect(ui->chkBoxDisplayFPS, SIGNAL(stateChanged(int)), this, SLOT(onDisplayFPSChanged(int)));

    // init buttons
    ui->btnRecordAlways->setColor(QColor(COLOR_LIGHT,0,0));
    ui->btnRecordAlways->setCheckedColor(QColor(COLOR_LIGHT+SEL_CELL_CLR_DELTA,0,0));
    ui->btnRecordMotion->setColor(QColor(0, COLOR_LIGHT, 0));
    ui->btnRecordMotion->setCheckedColor(QColor(0, COLOR_LIGHT+SEL_CELL_CLR_DELTA, 0));
    int noRecClr = COLOR_LIGHT - 24;
    ui->btnNoRecord->setColor(QColor(noRecClr, noRecClr, noRecClr));
    ui->btnNoRecord->setCheckedColor(QColor(noRecClr+SEL_CELL_CLR_DELTA, noRecClr+SEL_CELL_CLR_DELTA, noRecClr+SEL_CELL_CLR_DELTA));

    connect(ui->btnRecordAlways, SIGNAL(toggled(bool)), this, SLOT(updateGridParams()));
    connect(ui->btnRecordMotion, SIGNAL(toggled(bool)), this, SLOT(updateGridParams()));
    connect(ui->btnNoRecord, SIGNAL(toggled(bool)), this, SLOT(updateGridParams()));
    connect(ui->comboBoxQuality, SIGNAL(currentIndexChanged(int)), this, SLOT(updateGridParams()));
    connect(ui->fpsSpinBox, SIGNAL(valueChanged(double)), this, SLOT(updateGridParams()));

    connect(ui->gridWidget, SIGNAL(needReadCellParams(QPoint)), this, SLOT(onNeedReadCellParams(QPoint)));

    m_disableUpdateGridParams = false;
}

QString getShortText(const QString& text)
{
    if (text == "Low")
        return "Lo";
    else if (text == "Medium")
        return "Md";
    else if (text == "High")
        return "Hi";
    else if (text == "Best")
        return "Bst";
    else 
        return "-";
}

QString getLongText(const QString& text)
{
    if (text == "Lo")
        return "Low";
    else if (text == "Md")
        return "Medium";
    else if (text == "Hi")
        return "High";
    else if (text == "Bst")
        return "Best";
    else 
        return "-";
}

int CameraScheduleWidget::qualityTextToIndex(const QString& text)
{
    for (int i = 0; i < ui->comboBoxQuality->count(); ++i)
    {
        if (ui->comboBoxQuality->itemText(i) == text)
            return i;
    }
    return 0;
}

void CameraScheduleWidget::updateGridParams()
{
    if (m_disableUpdateGridParams)
        return;

    QColor color;
    if (ui->btnRecordAlways->isChecked())
        color = ui->btnRecordAlways->color();
    else if (ui->btnRecordMotion->isChecked())
        color = ui->btnRecordMotion->color();
    else if (ui->btnNoRecord->isChecked()) 
        color = ui->btnNoRecord->color();

    ui->gridWidget->setDefaultParam(QnScheduleGridWidget::ParamType_Color, color.rgba());
    if (ui->btnNoRecord->isChecked()) {
        ui->gridWidget->setDefaultParam(QnScheduleGridWidget::ParamType_First, QString("-"));
        ui->gridWidget->setDefaultParam(QnScheduleGridWidget::ParamType_Second, QString("-"));
    }
    else 
    {
        ui->gridWidget->setDefaultParam(QnScheduleGridWidget::ParamType_First, QString::number(ui->fpsSpinBox->value()));
        ui->gridWidget->setDefaultParam(QnScheduleGridWidget::ParamType_Second, getShortText(ui->comboBoxQuality->currentText()));
    }
}

void CameraScheduleWidget::onNeedReadCellParams(QPoint cell)
{
    m_disableUpdateGridParams = true;
    QColor color(ui->gridWidget->getCellParam(cell, QnScheduleGridWidget::ParamType_Color).toUInt());
    double fps(ui->gridWidget->getCellParam(cell, QnScheduleGridWidget::ParamType_First).toDouble());
    QString shortQuality(ui->gridWidget->getCellParam(cell, QnScheduleGridWidget::ParamType_Second).toString());

    if (color == ui->btnRecordAlways->color())
        ui->btnRecordAlways->setChecked(true);
    else if (color == ui->btnRecordMotion->color())
        ui->btnRecordMotion->setChecked(true);
    else if (color == ui->btnNoRecord->color())
        ui->btnNoRecord->setChecked(true);
    if (color != ui->btnNoRecord->color()) 
    {
        ui->fpsSpinBox->setValue(fps);
        ui->comboBoxQuality->setCurrentIndex(qualityTextToIndex(getLongText(shortQuality)));
    }
    m_disableUpdateGridParams = false;
    updateGridParams();
}

void CameraScheduleWidget::onDisplayQualityChanged(int state)
{
    ui->gridWidget->setShowSecondParam(state);
}

void CameraScheduleWidget::onDisplayFPSChanged(int state)
{
    ui->gridWidget->setShowFirstParam(state);
}
