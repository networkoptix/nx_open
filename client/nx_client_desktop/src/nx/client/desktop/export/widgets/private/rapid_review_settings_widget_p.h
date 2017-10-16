#pragma once

#include <QtCore/QObject>

class QStandardItemModel;

namespace nx {
namespace client {
namespace desktop {

class RapidReviewSettingsWidgetPrivate: public QObject
{
    using base_type = QObject;
public:
    RapidReviewSettingsWidgetPrivate(QObject* parent = nullptr);

    QStandardItemModel* unitsModel;

    // Minimal rapid review speed. Limited by keyframes count.
    static constexpr int kMinimalSpeed = 10;

    // Minimal result length.
    static constexpr qint64 kMinimalLengthMs = 1000;

    int absoluteSpeed() const;
    void setAbsoluteSpeed(int absoluteSpeed);
    static int minAbsoluteSpeed();
    int maxAbsoluteSpeed() const;

    int sliderSpeed() const;
    void setSliderSpeed(int sliderSpeed);
    int minSliderSpeed() const;
    int maxSliderSpeed() const;

    int selectedLengthUnitIndex() const;
    void setSelectedLengthUnit(int index);

    qint64 sourcePeriodLengthMs() const;
    void setSourcePeriodLengthMs(qint64 lengthMs);

    qreal resultLengthInSelectedUnits() const;
    void setResultLengthInSelectedUnits(qreal value);
    qreal minResultLengthInSelectedUnits() const;
    qreal maxResultLengthInSelectedUnits() const;

    qint64 frameStepMs() const;

    bool isSourceRangeValid() const;

private:
    void updateUnitsModel();
    void updateExpectedLength();
    void updateResultLengthInSelectedUnitsRange();

    qreal sliderExpCoeff() const;
    qreal toSliderScale(qreal absoluteSpeedValue) const;
    qreal fromSliderScale(qreal sliderSpeedValue) const;

    qint64 selectedLengthMeasureUnitInMs() const;

private:
    qreal m_absoluteSpeed;
    qreal m_sliderSpeed;
    int m_maxSpeed;
    qint64 m_sourcePeriodLengthMs;
    int m_selectedLengthUnitIndex;
    qreal m_resultLengthInSelectedUnits;
    qreal m_minResultLengthInSelectedUnits;
    qreal m_maxResultLengthInSelectedUnits;
    bool m_sourceRangeValid = true;
};

} // namespace desktop
} // namespace client
} // namespace nx
