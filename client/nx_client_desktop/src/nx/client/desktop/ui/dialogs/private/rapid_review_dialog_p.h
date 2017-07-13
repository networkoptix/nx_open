#pragma once

#include <QtCore/QObject>

class QStandardItemModel;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace dialogs {

class ExportRapidReviewPrivate: public QObject
{
    using base_type = QObject;
public:
    ExportRapidReviewPrivate(QObject* parent = nullptr);

    QStandardItemModel* unitsModel;

    // Minimal rapid review speed. Limited by keyframes count.
    static constexpr int kMinimalSpeed = 10;

    // Minimal result length.
    static constexpr qint64 kMinimalLengthMs = 1000;

    int absoluteSpeed() const;
    void setAbsoluteSpeed(int absoluteSpeed);
    int minAbsoluteSpeed() const;
    int maxAbsoluteSpeed() const;

    int sliderSpeed() const;
    void setSliderSpeed(int sliderSpeed);
    int minSliderSpeed() const;
    int maxSliderSpeed() const;

    int selectedLengthUnit() const;
    void setSelectedLengthUnit(int index);

    qint64 sourcePeriodLengthMs() const;
    void setSourcePeriodLengthMs(qint64 lengthMs);

    double resultLengthUnits() const;
    void setResultLengthUnits(double value);
    double minResultLengthUnits() const;
    double maxResultLengthUnits() const;

    qint64 frameStepMs() const;

private:
    void updateUnitsModel();
    void updateExpectedLength();
    void updateResultLengthUnitsRange();

    double sliderExpCoeff() const;
    double toSliderScale(double absoluteSpeedValue) const;
    double fromSliderScale(double sliderSpeedValue) const;

    qint64 expectedLengthMeasureUnit() const;

private:
    double m_absoluteSpeed;
    double m_sliderSpeed;
    int m_maxSpeed;
    qint64 m_sourcePeriodLengthMs;
    int m_selectedLengthUnit;
    double m_resultLengthUnits;
    double m_minResultLengthUnits;
    double m_maxResultLengthUnits;
};

} // namespace dialogs
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
