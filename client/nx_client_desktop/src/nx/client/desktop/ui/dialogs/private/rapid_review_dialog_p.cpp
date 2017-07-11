#include "rapid_review_dialog_p.h"
#include "../rapid_review_dialog.h"

#include <array>

#include <QtGui/QStandardItemModel>

#include <text/time_strings.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>

namespace {

// Maximal rapid review speed - one year in a minute.
static constexpr int kMaximalSpeed = 365 * 24 * 60;

// Default value for timelapse video: 5 minutes.
static constexpr qint64 kDefaultLengthMs = 5 * 60 * 1000;

// Default value for slider steps number.
static constexpr int kSliderSteps = 20;

static constexpr qint64 kMsInSec = 1000;
static constexpr qint64 kMsInMin = kMsInSec * 60;
static constexpr qint64 kMsInHour = kMsInMin * 60;
static constexpr qint64 kMsInDay = kMsInHour * 24;

static constexpr int kLevelCount = 4;

static constexpr std::array<qint64, kLevelCount> kUnits = {kMsInSec, kMsInMin, kMsInHour, kMsInDay};

// After this value units will be changed to bigger one
static constexpr std::array<int, kLevelCount - 1> kMaxValuePerUnit = {120, 120, 720};

void addModelItem(const QString& text, qint64 measureUnit, QStandardItemModel* model)
{
    auto item = new QStandardItem(text);
    item->setData(measureUnit);
    model->appendRow(item);
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace dialogs {

ExportRapidReviewPrivate::ExportRapidReviewPrivate(QObject* parent):
    base_type(parent),
    unitsModel(new QStandardItemModel(this)),
    m_absoluteSpeed(kMinimalSpeed),
    m_sliderSpeed(kMinimalSpeed),
    m_maxSpeed(kMinimalSpeed), //< Nope, that is not an error :)
    m_sourcePeriodLengthMs(kMinimalLengthMs * kMinimalSpeed),
    m_selectedLengthUnit(0),
    m_minResultLengthUnits(1),
    m_maxResultLengthUnits(1)
{
    updateUnitsModel();
}

int ExportRapidReviewPrivate::absoluteSpeed() const
{
    return qRound(m_absoluteSpeed);
}

void ExportRapidReviewPrivate::setAbsoluteSpeed(int absoluteSpeed)
{
    // On speed change via spinbox we must modify slider speed and result time.
    m_absoluteSpeed = absoluteSpeed;
    m_sliderSpeed = toSliderScale(absoluteSpeed);
    updateExpectedLength();
}

int ExportRapidReviewPrivate::minAbsoluteSpeed() const
{
    return kMinimalSpeed;
}

int ExportRapidReviewPrivate::maxAbsoluteSpeed() const
{
    return m_maxSpeed;
}

int ExportRapidReviewPrivate::sliderSpeed() const
{
    return qRound(m_sliderSpeed);
}

void ExportRapidReviewPrivate::setSliderSpeed(int sliderSpeed)
{
    // On speed change via slider we must modify spinbox and result time.
    m_sliderSpeed = sliderSpeed;
    m_absoluteSpeed = fromSliderScale(sliderSpeed);
    updateExpectedLength();
}

int ExportRapidReviewPrivate::minSliderSpeed() const
{
    NX_EXPECT(toSliderScale(kMinimalSpeed) == 0);
    return 0;
}

int ExportRapidReviewPrivate::maxSliderSpeed() const
{
    return toSliderScale(m_maxSpeed);
}

int ExportRapidReviewPrivate::selectedLengthUnit() const
{
    return m_selectedLengthUnit;
}

void ExportRapidReviewPrivate::setSelectedLengthUnit(int index)
{
    // Changed length unit (minutes to seconds or vise versa). Speed must not change.
    m_selectedLengthUnit = index;

    updateResultLengthUnitsRange();

    const double expectedLengthMs = m_sourcePeriodLengthMs / m_absoluteSpeed;
    m_resultLengthUnits = expectedLengthMs / expectedLengthMeasureUnit();
}

qint64 ExportRapidReviewPrivate::sourcePeriodLengthMs() const
{
    return m_sourcePeriodLengthMs;
}

void ExportRapidReviewPrivate::setSourcePeriodLengthMs(qint64 lengthMs)
{
    if (m_sourcePeriodLengthMs == lengthMs)
        return;

    m_sourcePeriodLengthMs = lengthMs;
    m_maxSpeed = m_sourcePeriodLengthMs / kMinimalLengthMs;

    updateUnitsModel();

    m_absoluteSpeed = m_sourcePeriodLengthMs / kDefaultLengthMs;
    if (m_absoluteSpeed < kMinimalSpeed)
        m_absoluteSpeed = kMinimalSpeed;
    m_sliderSpeed = toSliderScale(m_absoluteSpeed);
    updateExpectedLength();
}

double ExportRapidReviewPrivate::resultLengthUnits() const
{
    return m_resultLengthUnits;
}

void ExportRapidReviewPrivate::setResultLengthUnits(double value)
{
    m_resultLengthUnits = value;

    const double expectedLengthMs = value * expectedLengthMeasureUnit();
    m_absoluteSpeed = kMaximalSpeed;
    if (value > 0)
        m_absoluteSpeed = qRound(m_sourcePeriodLengthMs / expectedLengthMs);
    m_sliderSpeed = toSliderScale(m_absoluteSpeed);
}

double ExportRapidReviewPrivate::minResultLengthUnits() const
{
    return m_minResultLengthUnits;
}

double ExportRapidReviewPrivate::maxResultLengthUnits() const
{
    return m_maxResultLengthUnits;
}

qint64 ExportRapidReviewPrivate::frameStepMs() const
{
    return 1000ll * m_absoluteSpeed / ExportRapidReview::kResultFps;
}

void ExportRapidReviewPrivate::updateUnitsModel()
{
    unitsModel->clear();

    const auto maxExpectedLengthMs =  m_sourcePeriodLengthMs / kMinimalSpeed;

    if (maxExpectedLengthMs >= kMsInSec)
    {
        addModelItem(QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Seconds), kMsInSec,
            unitsModel);
    }

    if (maxExpectedLengthMs >= kMsInMin)
    {
        addModelItem(QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Minutes), kMsInMin,
            unitsModel);
    }

    if (maxExpectedLengthMs >= kMsInHour)
    {
        addModelItem(QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Hours), kMsInHour,
            unitsModel);
    }

    if (maxExpectedLengthMs >= kMsInDay)
    {
        addModelItem(QnTimeStrings::longSuffix(QnTimeStrings::Suffix::Days), kMsInDay,
            unitsModel);
    }
}

void ExportRapidReviewPrivate::updateExpectedLength()
{
    const double expectedLengthMs = m_sourcePeriodLengthMs / m_absoluteSpeed;

    m_resultLengthUnits = expectedLengthMs / expectedLengthMeasureUnit();

    while (m_selectedLengthUnit >= 0
        && m_selectedLengthUnit < unitsModel->rowCount() - 1
        && m_resultLengthUnits >= kMaxValuePerUnit[m_selectedLengthUnit])
    {
        ++m_selectedLengthUnit;
        m_resultLengthUnits = expectedLengthMs / expectedLengthMeasureUnit();
    }

    while (m_resultLengthUnits < 1.0 && m_selectedLengthUnit > 0)
    {
        --m_selectedLengthUnit;
        m_resultLengthUnits = expectedLengthMs / expectedLengthMeasureUnit();
    }

    updateResultLengthUnitsRange();
}

void ExportRapidReviewPrivate::updateResultLengthUnitsRange()
{
    qint64 unit = expectedLengthMeasureUnit();

    m_minResultLengthUnits = std::max(1.0 * kMinimalLengthMs / (unit * kMinimalSpeed), 1.0);
    m_maxResultLengthUnits = std::max(1.0 * m_sourcePeriodLengthMs / (unit * kMinimalSpeed), 1.0);
}

double ExportRapidReviewPrivate::sliderExpCoeff() const
{
    /*
     * Function is: kMinimalSpeed * e ^ (k * kSliderSteps) == maxSpeed.
     * From here getting sliderExpCoeff = k. Then using formula:
     * kMinimalSpeed * e ^ (sliderExpCoeff * sliderValue) == absoluteSpeedValue.
     */

    double speedUp = m_maxSpeed / kMinimalSpeed;
    return log(speedUp) / kSliderSteps;
}

double ExportRapidReviewPrivate::toSliderScale(double absoluteSpeedValue) const
{
    //Assuming: kMinimalSpeed * e ^ (sliderExpCoeff * sliderValue) == absoluteSpeedValue.

    /* If we have less steps than recommended value, just use direct scale */
    if (m_maxSpeed - kMinimalSpeed <= kSliderSteps)
        return absoluteSpeedValue - kMinimalSpeed;

    const double k = sliderExpCoeff();

    const double targetSpeedUp = 1.0 * absoluteSpeedValue / kMinimalSpeed;
    const double sliderValue = log(targetSpeedUp) / k;
    return sliderValue;
}

double ExportRapidReviewPrivate::fromSliderScale(double sliderSpeedValue) const
{
    //Assuming: kMinimalSpeed * e ^ (sliderExpCoeff * sliderValue) == absoluteSpeedValue.

    // If we have less steps than recommended value, just use direct scale.
    if (m_maxSpeed - kMinimalSpeed <= kSliderSteps)
        return kMinimalSpeed + sliderSpeedValue;

    /* Negating rounding errors. */
    if (qFuzzyEquals(sliderSpeedValue, kSliderSteps))
        return m_maxSpeed;

    const double k = sliderExpCoeff();

    const double absoluteSpeed = 1.0 * kMinimalSpeed * exp(k * sliderSpeedValue);
    return absoluteSpeed;
}

qint64 ExportRapidReviewPrivate::expectedLengthMeasureUnit() const
{
    const auto index = std::max(m_selectedLengthUnit, 0);
    NX_EXPECT(index == m_selectedLengthUnit);

    NX_EXPECT(unitsModel->rowCount() > index, Q_FUNC_INFO, "Make sure model is valid");
    if (index >= unitsModel->rowCount())
        return 1;

    qint64 measureUnit = unitsModel->item(index)->data().toLongLong();
    return std::max(measureUnit, 1ll);
}

} // namespace dialogs
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
