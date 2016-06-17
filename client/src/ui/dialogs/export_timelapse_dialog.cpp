#include "export_timelapse_dialog.h"
#include "ui_export_timelapse_dialog.h"

#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/scoped_value_rollback.h>

namespace {

/* Font size for info labels. */
const int kInfoFontSize = 16;

/* Minimal timelapse speed. Limited by keyframes count. */
const int kMinimalSpeed = 10;

/* Maximal timelapse speed - one year in a minute. */
const int kMaximalSpeed = 365 * 24 * 60;

/* Minimal result length. */
const qint64 kMinimalLengthMs = 1000;

/* Sane default value for timelapse video: 5 minutes */
const qint64 kDefaultLengthMs = 5 * 60 * 1000;

/* Magic knowledge. We know that the result will be generated with 30 fps. --rvasilenko */
const int kResultFps = 30;

/* Default value for slider steps number. */
const int kSliderSteps = 20;

}

qint64 QnExportTimelapseDialog::kMinimalSourcePeriodLength = kMinimalLengthMs * kMinimalSpeed;

QnExportTimelapseDialog::QnExportTimelapseDialog(QWidget *parent, Qt::WindowFlags windowFlags) :
    base_type(parent, windowFlags),
    ui(new Ui::ExportTimelapseDialog),
    m_updating(false),
    m_expectedLengthMs(kDefaultLengthMs),
    m_sourcePeriodLengthMs(kMinimalLengthMs * kMinimalSpeed),
    m_frameStepMs(0)
{
    ui->setupUi(this);

    QFont infoFont(this->font());
    infoFont.setPixelSize(kInfoFontSize);
    //infoFont.setBold(true);

    ui->initialLengthLabel->setFont(infoFont);
    ui->framesLabel->setFont(infoFont);

    QStandardItemModel* unitsModel = new QStandardItemModel(this);
    unitsModel->appendRow(new QStandardItem(tr("sec")));
    unitsModel->appendRow(new QStandardItem(tr("min")));
    unitsModel->appendRow(new QStandardItem(tr("hrs")));

    ui->resultLengthUnitsComboBox->setModel(unitsModel);

    connect(ui->speedSpinBox, QnSpinboxIntValueChanged, this, [this](int value)
    {
        if (m_updating)
            return;

        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        ui->speedSlider->setValue(toSliderScale(value));
        setExpectedLength(m_sourcePeriodLengthMs / value);
    });

    connect(ui->speedSlider, &QSlider::valueChanged, this, [this](int value)
    {
        if (m_updating)
            return;

        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        int absoluteValue = fromSliderScale(value);
        ui->speedSpinBox->setValue(absoluteValue);
        setExpectedLength(m_sourcePeriodLengthMs / absoluteValue);
    });

    initControls();
}

QnExportTimelapseDialog::~QnExportTimelapseDialog()
{}

qint64 QnExportTimelapseDialog::sourcePeriodLengthMs() const
{
    return m_sourcePeriodLengthMs;
}

void QnExportTimelapseDialog::setSourcePeriodLengthMs(qint64 lengthMs)
{
    if (m_sourcePeriodLengthMs == lengthMs)
        return;
    m_sourcePeriodLengthMs = lengthMs;
    initControls();
}

qint64 QnExportTimelapseDialog::frameStepMs() const
{
    return m_frameStepMs;
}

void QnExportTimelapseDialog::initControls()
{
    if (m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    int maxSpeed = m_sourcePeriodLengthMs / kMinimalLengthMs;
    ui->speedSpinBox->setRange(kMinimalSpeed, maxSpeed);
    {
        int maxRange = toSliderScale(maxSpeed);
        qDebug() << "maximum range" << maxRange;
        qDebug() << "back check" << fromSliderScale(maxRange) << " diff to" << maxSpeed;
    }
    ui->speedSlider->setRange(0, toSliderScale(maxSpeed));
    Q_ASSERT(toSliderScale(kMinimalSpeed) == 0);

    qint64 maxExpectedLengthMs = m_sourcePeriodLengthMs / kMinimalSpeed;
    ui->resultLengthSpinBox->setMaximum(static_cast<int>(maxExpectedLengthMs / 1000));

    qint64 expectedLengthMs = kDefaultLengthMs;
    int speed = m_sourcePeriodLengthMs / expectedLengthMs;
    if (speed < kMinimalSpeed)
    {
        speed = kMinimalSpeed;
        expectedLengthMs = m_sourcePeriodLengthMs / speed;
    }
    ui->speedSpinBox->setValue(speed);
    ui->speedSlider->setValue(toSliderScale(speed));
    setExpectedLength(expectedLengthMs);

}

void QnExportTimelapseDialog::setExpectedLength(qint64 value)
{
    m_expectedLengthMs = value;
    qint64 framesCount = m_sourcePeriodLengthMs / value;
    m_frameStepMs = framesCount * 1000 / kResultFps;
    ui->framesLabel->setText(QString::number(m_frameStepMs));
    ui->resultLengthSpinBox->setValue(static_cast<int>(value / 1000));
}

int QnExportTimelapseDialog::toSliderScale(int absoluteSpeedValue)
{
    /* If we have less steps than recommended value, just use direct scale */
    int maxSpeed = m_sourcePeriodLengthMs / kMinimalLengthMs;
    if (maxSpeed - kMinimalSpeed <= kSliderSteps)
        return absoluteSpeedValue - kMinimalSpeed;

    /*TODO: #GDM Function is: kMinimalSpeed * e ^ (k * kSliderSteps) == maxSpeed. From here getting k. */

    return static_cast<int>(log(absoluteSpeedValue));
}

int QnExportTimelapseDialog::fromSliderScale(int sliderValue)
{
    /* If we have less steps than recommended value, just use direct scale */
    int maxSpeed = m_sourcePeriodLengthMs / kMinimalLengthMs;
    if (maxSpeed - kMinimalSpeed <= kSliderSteps)
        return kMinimalSpeed + sliderValue;

    /*TODO: #GDM Function is: kMinimalSpeed * e ^ (k * kSliderSteps) == maxSpeed. From here getting k. */

    return static_cast<int>(exp(sliderValue));
}
