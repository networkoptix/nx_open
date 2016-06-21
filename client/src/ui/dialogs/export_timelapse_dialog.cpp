#include "export_timelapse_dialog.h"
#include "ui_export_timelapse_dialog.h"

#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/common/qtimespan.h>
#include <array>

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

/* Default value for slider steps number. */
const int kSliderSteps = 20;

void addModelItem(const QString& text, qint64 measureUnit, QStandardItemModel* model)
{
    QStandardItem* item = new QStandardItem(text);
    item->setData(measureUnit);
    model->appendRow(item);
}

/*
 * Function is: kMinimalSpeed * e ^ (k * kSliderSteps) == maxSpeed.
 * From here getting sliderExpCoeff = k. Then using formula:
 * kMinimalSpeed * e ^ (sliderExpCoeff * sliderValue) == absoluteSpeedValue.
 */
qreal sliderExpCoeff(qreal maxSpeed)
{
    qreal speedUp = maxSpeed / kMinimalSpeed;
    return log(speedUp) / kSliderSteps;
}

} //anonymous namespace

qint64 QnExportTimelapseDialog::kMinimalSourcePeriodLength = kMinimalLengthMs * kMinimalSpeed;

QnExportTimelapseDialog::QnExportTimelapseDialog(QWidget *parent, Qt::WindowFlags windowFlags) :
    base_type(parent, windowFlags),
    ui(new Ui::ExportTimelapseDialog),
    m_updating(false),
    m_expectedLengthMs(kDefaultLengthMs),
    m_sourcePeriodLengthMs(kMinimalLengthMs * kMinimalSpeed),
    m_frameStepMs(0),
    m_maxSpeed(m_sourcePeriodLengthMs / kMinimalLengthMs),
    m_filteredUnitsModel(new QStandardItemModel(this))
{
    ui->setupUi(this);

    QFont infoFont(this->font());
    infoFont.setPixelSize(kInfoFontSize);

    ui->initialLengthLabel->setFont(infoFont);
    ui->framesLabel->setFont(infoFont);

    ui->resultLengthUnitsComboBox->setModel(m_filteredUnitsModel);

    connect(ui->speedSpinBox, QnSpinboxIntValueChanged, this, [this](int absoluteValue)
    {
        if (m_updating)
            return;

        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        ui->speedSlider->setValue(toSliderScale(absoluteValue));
        setExpectedLengthMs(m_sourcePeriodLengthMs / absoluteValue);
    });

    connect(ui->speedSlider, &QSlider::valueChanged, this, [this](int value)
    {
        if (m_updating)
            return;

        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        int absoluteValue = fromSliderScale(value);
        ui->speedSpinBox->setValue(absoluteValue);
        setExpectedLengthMs(m_sourcePeriodLengthMs / absoluteValue);
    });

    auto expectedLengthChanged = [this]()
    {
        if (m_updating)
            return;

        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

        int index = ui->resultLengthUnitsComboBox->currentIndex();
        qint64 measureUnit = m_filteredUnitsModel->item(index)->data().toLongLong();
        qint64 absoluteValue = kMaximalSpeed;
        if (ui->resultLengthSpinBox->value() > 0)
            absoluteValue = m_sourcePeriodLengthMs / (ui->resultLengthSpinBox->value() * measureUnit);
        ui->speedSlider->setValue(toSliderScale(absoluteValue));
        ui->speedSpinBox->setValue(absoluteValue);

        updateFrameStep(absoluteValue);
    };

    connect(ui->resultLengthSpinBox, QnSpinboxIntValueChanged, this, expectedLengthChanged);
    connect(ui->resultLengthUnitsComboBox, QnComboboxCurrentIndexChanged, this, expectedLengthChanged);


    initControls();
}

void QnExportTimelapseDialog::setSpeed(qint64 absoluteValue)
{
    ui->speedSpinBox->setValue(absoluteValue);
    ui->speedSlider->setValue(toSliderScale(absoluteValue));
    setExpectedLengthMs(m_sourcePeriodLengthMs / absoluteValue);
}

qint64 QnExportTimelapseDialog::speed() const
{
    return ui->speedSpinBox->value();
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

    m_filteredUnitsModel->clear();

    static const qint64 msInSec = 1000;
    static const qint64 msInMin = 1000 * 60;
    static const qint64 msInHour = 1000 * 60 * 60;
    static const qint64 msInDay = 1000 * 60 * 60 * 24;

    qint64 maxExpectedLengthMs = m_sourcePeriodLengthMs / kMinimalSpeed;
    if (maxExpectedLengthMs >= msInSec)
        addModelItem(tr("sec"), msInSec,  m_filteredUnitsModel);
    if (maxExpectedLengthMs >= msInMin)
        addModelItem(tr("min"), msInMin,  m_filteredUnitsModel);
    if (maxExpectedLengthMs >= msInHour)
        addModelItem(tr("hrs"), msInHour, m_filteredUnitsModel);
    if (maxExpectedLengthMs >= msInDay)
        addModelItem(tr("days"), msInDay,  m_filteredUnitsModel);

    m_maxSpeed = m_sourcePeriodLengthMs / kMinimalLengthMs;
    ui->speedSpinBox->setRange(kMinimalSpeed, m_maxSpeed);
    ui->speedSlider->setRange(toSliderScale(kMinimalSpeed), toSliderScale(m_maxSpeed));
    Q_ASSERT(toSliderScale(kMinimalSpeed) == 0);

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
    setExpectedLengthMs(expectedLengthMs);
    ui->initialLengthLabel->setText(durationMsToString(m_sourcePeriodLengthMs));
}

void QnExportTimelapseDialog::setExpectedLengthMs(qint64 value)
{
    int speed = m_sourcePeriodLengthMs / value;
    updateFrameStep(speed);
    int index = ui->resultLengthUnitsComboBox->currentIndex();
    qint64 measureUnit = m_filteredUnitsModel->item(index)->data().toLongLong();
    ui->resultLengthSpinBox->setValue(static_cast<int>(value / measureUnit));
}

void QnExportTimelapseDialog::updateFrameStep(int speed)
{
    m_expectedLengthMs = speed;
    m_frameStepMs = 1000ll * speed / kResultFps;
    ui->framesLabel->setText(durationMsToString(m_frameStepMs));
}

/* kMinimalSpeed * e ^ (sliderExpCoeff * sliderValue) == absoluteSpeedValue. */
int QnExportTimelapseDialog::toSliderScale(int absoluteSpeedValue)
{
    /* If we have less steps than recommended value, just use direct scale */
    int m_maxSpeed = m_sourcePeriodLengthMs / kMinimalLengthMs;
    if (m_maxSpeed - kMinimalSpeed <= kSliderSteps)
        return absoluteSpeedValue - kMinimalSpeed;

    qreal k = sliderExpCoeff(m_maxSpeed);

    qreal targetSpeedUp = 1.0 * absoluteSpeedValue / kMinimalSpeed;
    qreal sliderValue = log(targetSpeedUp) / k;
    return static_cast<int>(sliderValue);
}

/* kMinimalSpeed * e ^ (sliderExpCoeff * sliderValue) == absoluteSpeedValue. */
int QnExportTimelapseDialog::fromSliderScale(int sliderValue)
{
    /* If we have less steps than recommended value, just use direct scale */
    if (m_maxSpeed - kMinimalSpeed <= kSliderSteps)
        return kMinimalSpeed + sliderValue;

    /* Negating rounding errors. */
    if (sliderValue == kSliderSteps)
        return m_maxSpeed;

    qreal k = sliderExpCoeff(m_maxSpeed);

    qreal absoluteSpeed = 1.0 * kMinimalSpeed * exp(k * sliderValue);
    return static_cast<int>(absoluteSpeed);
}

QString QnExportTimelapseDialog::durationMsToString(qint64 durationMs)
{
    auto unitStringsConverter = [](Qt::TimeSpanUnit unit, int num)
    {
        switch (unit) {
        case::Qt::Milliseconds:
            return tr("%nms", "", num);
        case::Qt::Seconds:
            return tr("%ns", "", num);
        case::Qt::Minutes:
            return tr("%nm", "", num);
        case::Qt::Hours:
            return tr("%nh", "", num);
        case::Qt::Days:
            return tr("%nd", "", num);
        default:
            return QString();
        }
    };

    /* Make sure passed format is fully covered by converter. */
    return QTimeSpan(durationMs).toApproximateString(3, Qt::DaysAndTime | Qt::Milliseconds, unitStringsConverter, lit(" "));
}
