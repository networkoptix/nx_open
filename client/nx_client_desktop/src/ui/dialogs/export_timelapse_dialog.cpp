#include "export_timelapse_dialog.h"
#include "ui_export_timelapse_dialog.h"

#include <array>

#include <QtGui/QStandardItemModel>

#include <text/time_strings.h>

#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/common/qtimespan.h>

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

constexpr qint64 kMsInSec = 1000;
constexpr qint64 kMsInMin = kMsInSec * 60;
constexpr qint64 kMsInHour = kMsInMin * 60;
constexpr qint64 kMsInDay = kMsInHour * 24;

constexpr int kLevelCount = 4;

constexpr std::array<qint64, kLevelCount> kUnits = {kMsInSec, kMsInMin, kMsInHour, kMsInDay};

// After this value units will be changed to bigger one
constexpr std::array<int, kLevelCount - 1> kMaxValuePerUnit = {120, 120, 720};

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
    m_speed(kMinimalSpeed),
    m_sourcePeriodLengthMs(kMinimalLengthMs * kMinimalSpeed),
    m_frameStepMs(0),
    m_maxSpeed(m_sourcePeriodLengthMs / kMinimalLengthMs),
    m_filteredUnitsModel(new QStandardItemModel(this))
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::Rapid_Review_Help);

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

    // TODO: Refactor this in 3.1
    auto resultLengthChangedInternal =
        [this](double value)
        {
            qint64 absoluteValue = kMaximalSpeed;
            if (value > 0)
                absoluteValue = m_sourcePeriodLengthMs / (value * expectedLengthMeasureUnit());

            ui->speedSlider->setValue(toSliderScale(absoluteValue));
            ui->speedSpinBox->setValue(absoluteValue);

            updateFrameStepInternal(absoluteValue);
        };

    connect(ui->resultLengthSpinBox, QnSpinboxDoubleValueChanged, this,
        [this, resultLengthChangedInternal](int value)
        {
            if (m_updating)
                return;

            QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
            resultLengthChangedInternal(value);
        });

    connect(ui->resultLengthUnitsComboBox, QnComboboxCurrentIndexChanged, this,
        [this, resultLengthChangedInternal](int value)
        {
            if (m_updating)
                return;

            QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

            updateExpectedLengthRangeInternal(value);
            const qreal expectedLength = m_sourcePeriodLengthMs / m_speed;

            // Here we must adjust time value to keep speed as near to previous as possible
            qreal valueInUnits = expectedLength / expectedLengthMeasureUnit(value);
            ui->resultLengthSpinBox->setValue(valueInUnits);
            resultLengthChangedInternal(ui->resultLengthSpinBox->value());
        });

    initControls();
}

void QnExportTimelapseDialog::setSpeed(qint64 absoluteValue)
{
    NX_ASSERT(!m_updating, Q_FUNC_INFO, "Function belongs to outer interface, must not be called internally.");
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

    qint64 maxExpectedLengthMs = m_sourcePeriodLengthMs / kMinimalSpeed;
    if (maxExpectedLengthMs >= kMsInSec)
    {
        addModelItem(QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Seconds), kMsInSec,
            m_filteredUnitsModel);
    }
    if (maxExpectedLengthMs >= kMsInMin)
    {
        addModelItem(QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Minutes), kMsInMin,
            m_filteredUnitsModel);
    }
    if (maxExpectedLengthMs >= kMsInHour)
    {
        addModelItem(QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Hours), kMsInHour,
            m_filteredUnitsModel);
    }
    if (maxExpectedLengthMs >= kMsInDay)
    {
        addModelItem(QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Days), kMsInDay,
            m_filteredUnitsModel);
    }

    m_maxSpeed = m_sourcePeriodLengthMs / kMinimalLengthMs;
    ui->speedSpinBox->setRange(kMinimalSpeed, m_maxSpeed);
    ui->speedSlider->setRange(toSliderScale(kMinimalSpeed), toSliderScale(m_maxSpeed));
    Q_ASSERT(toSliderScale(kMinimalSpeed) == 0);

    ui->resultLengthSpinBox->setMaximum(1.0 * maxExpectedLengthMs / 1000);

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
    NX_ASSERT(m_updating, Q_FUNC_INFO, "Internal functions are to be called under guard.");

    int speed = m_sourcePeriodLengthMs / value;
    updateFrameStepInternal(speed);

    int index = ui->resultLengthUnitsComboBox->currentIndex();
    qreal valueInUnits = value * 1.0 / expectedLengthMeasureUnit(index);

    while (index >= 0
        && index < m_filteredUnitsModel->rowCount() - 1
        && valueInUnits >= kMaxValuePerUnit[index])
    {
        ++index;
        valueInUnits = value * 1.0 / expectedLengthMeasureUnit(index);
    }

    while (valueInUnits < 1.0 && index > 0)
    {
        --index;
        valueInUnits = value * 1.0 / expectedLengthMeasureUnit(index);
    }
    ui->resultLengthUnitsComboBox->setCurrentIndex(index);
    updateExpectedLengthRangeInternal(index);

    ui->resultLengthSpinBox->setValue(valueInUnits);
}

void QnExportTimelapseDialog::updateFrameStepInternal(int speed)
{
    NX_ASSERT(m_updating, Q_FUNC_INFO, "Internal functions are to be called under guard.");

    m_speed = speed;
    m_frameStepMs = 1000ll * speed / kResultFps;
    ui->framesLabel->setText(durationMsToString(m_frameStepMs));
}

void QnExportTimelapseDialog::updateExpectedLengthRangeInternal(int index)
{
    NX_ASSERT(m_updating, Q_FUNC_INFO, "Internal functions are to be called under guard.");

    qint64 unit = expectedLengthMeasureUnit(index);

    double minExpectedLengthInUnits = std::max(1.0 * kMinimalLengthMs / (unit * kMinimalSpeed), 1.0);
    double maxExpectedLengthInUnits = std::max(1.0 * m_sourcePeriodLengthMs / (unit * kMinimalSpeed), 1.0);

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
    static const QString kSeparator(L' ');
    return QTimeSpan(durationMs).toApproximateString(3, Qt::DaysAndTime | Qt::Milliseconds,
        QTimeSpan::SuffixFormat::Short, kSeparator);
}

qint64 QnExportTimelapseDialog::expectedLengthMeasureUnit(int index) const
{
    if (index < 0)
        index = std::max(ui->resultLengthUnitsComboBox->currentIndex(), 0);

    NX_ASSERT(m_filteredUnitsModel->rowCount() > index, Q_FUNC_INFO, "Make sure model is valid");
    if (index >= m_filteredUnitsModel->rowCount())
        return 1;

    qint64 measureUnit = m_filteredUnitsModel->item(index)->data().toLongLong();
    return std::max(measureUnit, 1ll);
}
