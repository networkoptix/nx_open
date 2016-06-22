#include "export_timelapse_dialog.h"
#include "ui_export_timelapse_dialog.h"

#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/common/qtimespan.h>
#include <array>

namespace {

/* Font size for info labels. */
const int kInfoFontPixelSize = 16;

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
    infoFont.setPixelSize(kInfoFontPixelSize);

    ui->initialLengthLabel->setFont(infoFont);
    ui->framesLabel->setFont(infoFont);

    ui->resultLengthUnitsComboBox->setModel(m_filteredUnitsModel);

    connect(ui->speedSpinBox, QnSpinboxIntValueChanged, this, [this](int absoluteValue)
    {
        if (m_updating)
            return;

        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        ui->speedSlider->setValue(toSliderScale(absoluteValue));
        setExpectedLengthMsInternal(m_sourcePeriodLengthMs / absoluteValue);
    });

    connect(ui->speedSlider, &QSlider::valueChanged, this, [this](int value)
    {
        if (m_updating)
            return;

        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        int absoluteValue = fromSliderScale(value);
        ui->speedSpinBox->setValue(absoluteValue);
        setExpectedLengthMsInternal(m_sourcePeriodLengthMs / absoluteValue);
    });

    auto resultLengthChangedInternal = [this](int value)
    {
        qint64 absoluteValue = kMaximalSpeed;
        if (value > 0)
            absoluteValue = m_sourcePeriodLengthMs / (value * expectedLengthMeasureUnit());

        ui->speedSlider->setValue(toSliderScale(absoluteValue));
        ui->speedSpinBox->setValue(absoluteValue);

        updateFrameStepInternal(absoluteValue);
    };

    connect(ui->resultLengthSpinBox, QnSpinboxIntValueChanged, this, [this, resultLengthChangedInternal](int value)
    {
        if (m_updating)
            return;

        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        resultLengthChangedInternal(value);
    });

    connect(ui->resultLengthUnitsComboBox, QnComboboxCurrentIndexChanged, this, [this, resultLengthChangedInternal](int value)
    {
        if (m_updating)
            return;

        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        updateExpectedLengthRangeInternal(value);
        resultLengthChangedInternal(ui->resultLengthSpinBox->value());
    });

    initControls();
}

void QnExportTimelapseDialog::setSpeed(qint64 absoluteValue)
{
    Q_ASSERT_X(!m_updating, Q_FUNC_INFO, "Function belongs to outer interface, must not be called internally.");
    if (m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    ui->speedSpinBox->setValue(absoluteValue);
    ui->speedSlider->setValue(toSliderScale(absoluteValue));
    setExpectedLengthMsInternal(m_sourcePeriodLengthMs / absoluteValue);
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
        addModelItem(tr("sec"), msInSec, m_filteredUnitsModel);
    if (maxExpectedLengthMs >= msInMin)
        addModelItem(tr("min"), msInMin, m_filteredUnitsModel);
    if (maxExpectedLengthMs >= msInHour)
        addModelItem(tr("hrs"), msInHour, m_filteredUnitsModel);
    if (maxExpectedLengthMs >= msInDay)
        addModelItem(tr("days"), msInDay, m_filteredUnitsModel);

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
    setExpectedLengthMsInternal(expectedLengthMs);
    ui->initialLengthLabel->setText(durationMsToString(m_sourcePeriodLengthMs));
}

void QnExportTimelapseDialog::setExpectedLengthMsInternal(qint64 value)
{
    Q_ASSERT_X(m_updating, Q_FUNC_INFO, "Internal functions are to be called under guard.");

    int speed = m_sourcePeriodLengthMs / value;
    updateFrameStepInternal(speed);

    int index = ui->resultLengthUnitsComboBox->currentIndex();
    qreal valueInUnits = value * 1.0 / expectedLengthMeasureUnit(index);
    while (valueInUnits < 1.0 && index > 0)
    {
        --index;
        valueInUnits = value * 1.0 / expectedLengthMeasureUnit(index);
    }
    ui->resultLengthUnitsComboBox->setCurrentIndex(index);
    updateExpectedLengthRangeInternal(index);

    ui->resultLengthSpinBox->setValue(static_cast<int>(valueInUnits));
}

void QnExportTimelapseDialog::updateFrameStepInternal(int speed)
{
    Q_ASSERT_X(m_updating, Q_FUNC_INFO, "Internal functions are to be called under guard.");

    m_expectedLengthMs = speed;
    m_frameStepMs = 1000ll * speed / kResultFps;
    ui->framesLabel->setText(durationMsToString(m_frameStepMs));
}

void QnExportTimelapseDialog::updateExpectedLengthRangeInternal(int index)
{
    Q_ASSERT_X(m_updating, Q_FUNC_INFO, "Internal functions are to be called under guard.");

    qint64 unit = expectedLengthMeasureUnit(index);

    int minExpectedLengthInUnits = static_cast<int>(std::max(kMinimalLengthMs / (unit * kMinimalSpeed), 1ll));
    int maxExpectedLengthInUnits = static_cast<int>(std::max(m_sourcePeriodLengthMs / (unit * kMinimalSpeed), 1ll));

    ui->resultLengthSpinBox->setRange(minExpectedLengthInUnits, maxExpectedLengthInUnits);
}

/* kMinimalSpeed * e ^ (sliderExpCoeff * sliderValue) == absoluteSpeedValue. */
int QnExportTimelapseDialog::toSliderScale(int absoluteSpeedValue) const
{
    /* If we have less steps than recommended value, just use direct scale */
    if (m_maxSpeed - kMinimalSpeed <= kSliderSteps)
        return absoluteSpeedValue - kMinimalSpeed;

    const qreal k = sliderExpCoeff(m_maxSpeed);

    const qreal targetSpeedUp = 1.0 * absoluteSpeedValue / kMinimalSpeed;
    const qreal sliderValue = log(targetSpeedUp) / k;
    return static_cast<int>(sliderValue);
}

/* kMinimalSpeed * e ^ (sliderExpCoeff * sliderValue) == absoluteSpeedValue. */
int QnExportTimelapseDialog::fromSliderScale(int sliderValue) const
{
    /* If we have less steps than recommended value, just use direct scale */
    if (m_maxSpeed - kMinimalSpeed <= kSliderSteps)
        return kMinimalSpeed + sliderValue;

    /* Negating rounding errors. */
    if (sliderValue == kSliderSteps)
        return m_maxSpeed;

    const qreal k = sliderExpCoeff(m_maxSpeed);

    const qreal absoluteSpeed = 1.0 * kMinimalSpeed * exp(k * sliderValue);
    return static_cast<int>(absoluteSpeed);
}

QString QnExportTimelapseDialog::durationMsToString(qint64 durationMs)
{
    auto unitStringsConverter = [](Qt::TimeSpanUnit unit, int num)
    {
        switch (unit)
        {
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

qint64 QnExportTimelapseDialog::expectedLengthMeasureUnit(int index) const
{
    if (index < 0)
        index = std::max(ui->resultLengthUnitsComboBox->currentIndex(), 0);

    Q_ASSERT_X(m_filteredUnitsModel->rowCount() > index, Q_FUNC_INFO, "Make sure model is valid");
    if (index >= m_filteredUnitsModel->rowCount())
        return 1;

    qint64 measureUnit = m_filteredUnitsModel->item(index)->data().toLongLong();
    return std::max(measureUnit, 1ll);
}
