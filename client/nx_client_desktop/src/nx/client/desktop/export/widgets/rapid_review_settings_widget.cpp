#include "rapid_review_settings_widget.h"
#include "ui_rapid_review_settings_widget.h"
#include "private/rapid_review_settings_widget_p.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QStandardItemModel>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <utils/common/qtimespan.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

/* Font size for info labels. */
const int kInfoFontPixelSize = 16;

QString durationMsToString(qint64 durationMs)
{
    static const QString kSeparator(L' ');
    return QTimeSpan(durationMs).toApproximateString(3, Qt::DaysAndTime | Qt::Milliseconds,
        QTimeSpan::SuffixFormat::Short, kSeparator);
}

} // namespace

RapidReviewSettingsWidget::RapidReviewSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::RapidReviewSettingsWidget()),
    d(new RapidReviewSettingsWidgetPrivate())
{
    ui->setupUi(this);

    ui->speedSlider->setProperty(style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));

    QFont infoFont(this->font());
    infoFont.setPixelSize(kInfoFontPixelSize);
    ui->initialLengthText->setFont(infoFont);
    ui->framesIntervalText->setFont(infoFont);
    ui->initialLengthText->setForegroundRole(QPalette::Text);
    ui->framesIntervalText->setForegroundRole(QPalette::Text);

    ui->resultLengthUnitsComboBox->setModel(d->unitsModel);

    setWarningStyle(ui->tooShortPeriodWarningLabel);
    ui->tooShortPeriodWarningLabel->setText(tr("For exporting as Rapid Review "
        "video length should be at least %n seconds.", "",
        minimalSourcePeriodLengthMs() / 1000));

    connect(ui->speedSpinBox, QnSpinboxIntValueChanged, this,
        [this](int absoluteValue)
        {
            if (!m_updating)
                setSpeed(absoluteValue);

            if (d->isSourceRangeValid())
                emit speedChanged(absoluteValue, frameStepMs());
        });

    connect(ui->speedSlider, &QSlider::valueChanged, this,
        [this](int value)
        {
            if (m_updating)
                return;

            QScopedValueRollback<bool> updatingGuard(m_updating, true);
            d->setSliderSpeed(value);
            updateRanges();
            updateControls();
        });

    connect(ui->resultLengthSpinBox, QnSpinboxDoubleValueChanged, this,
        [this](double value)
        {
            if (m_updating)
                return;

            QScopedValueRollback<bool> updatingGuard(m_updating, true);
            d->setResultLengthInSelectedUnits(value);
            updateRanges();
            updateControls();
        });

    connect(ui->resultLengthUnitsComboBox, QnComboboxCurrentIndexChanged, this,
        [this](int index)
        {
            if (m_updating)
                return;

            QScopedValueRollback<bool> updatingGuard(m_updating, true);
            d->setSelectedLengthUnit(index);
            updateRanges();
            updateControls();
        });

    setSourcePeriodLengthMs(minimalSourcePeriodLengthMs());

    ui->deleteButton->setIcon(qnSkin->icon(lit("buttons/trash.png")));

    connect(ui->deleteButton, &QPushButton::clicked,
        this, &RapidReviewSettingsWidget::deleteClicked);

    setHelpTopic(this, Qn::Rapid_Review_Help);
}

RapidReviewSettingsWidget::~RapidReviewSettingsWidget()
{
}

void RapidReviewSettingsWidget::setSpeed(int absoluteValue)
{
    NX_ASSERT(!m_updating, Q_FUNC_INFO,
        "Function belongs to outer interface, must not be called internally.");
    if (m_updating)
        return;

    QScopedValueRollback<bool> updatingGuard(m_updating, true);
    d->setAbsoluteSpeed(absoluteValue);
    updateRanges();
    updateControls();
}

int RapidReviewSettingsWidget::speed() const
{
    return d->isSourceRangeValid() ? ui->speedSpinBox->value() : 1;
}

qint64 RapidReviewSettingsWidget::sourcePeriodLengthMs() const
{
    return d->sourcePeriodLengthMs();
}

void RapidReviewSettingsWidget::setSourcePeriodLengthMs(qint64 lengthMs)
{
    NX_ASSERT(!m_updating, Q_FUNC_INFO,
        "Function belongs to outer interface, must not be called internally.");
    if (m_updating)
        return;

    QScopedValueRollback<bool> updatingGuard(m_updating, true);
    d->setSourcePeriodLengthMs(lengthMs);

    updateRanges();
    updateControls();

    ui->initialLengthText->setText(durationMsToString(lengthMs));

    ui->modeStackedWidget->setCurrentWidget(
        d->isSourceRangeValid() ? ui->mainPage : ui->specialPage);

    if (!d->isSourceRangeValid())
        emit speedChanged(1, 0);
}

qint64 RapidReviewSettingsWidget::frameStepMs() const
{
    return d->isSourceRangeValid() ? d->frameStepMs() : 0;
}

void RapidReviewSettingsWidget::updateRanges()
{
    NX_EXPECT(m_updating);

    ui->speedSpinBox->setRange(d->minAbsoluteSpeed(), d->maxAbsoluteSpeed());
    ui->speedSlider->setRange(d->minSliderSpeed(), d->maxSliderSpeed());
    ui->resultLengthSpinBox->setRange(d->minResultLengthInSelectedUnits(),
        d->maxResultLengthInSelectedUnits());
}

void RapidReviewSettingsWidget::updateControls()
{
    NX_EXPECT(m_updating);

    ui->speedSpinBox->setValue(d->absoluteSpeed());
    ui->speedSlider->setValue(d->sliderSpeed());
    ui->resultLengthUnitsComboBox->setCurrentIndex(d->selectedLengthUnitIndex());
    ui->resultLengthSpinBox->setValue(d->resultLengthInSelectedUnits());
    ui->framesIntervalText->setText(durationMsToString(d->frameStepMs()));
}

qint64 RapidReviewSettingsWidget::minimalSourcePeriodLengthMs()
{
    return RapidReviewSettingsWidgetPrivate::kMinimalLengthMs
        * RapidReviewSettingsWidgetPrivate::kMinimalSpeed;
}

} // namespace desktop
} // namespace client
} // namespace nx
