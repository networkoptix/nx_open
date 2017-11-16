#include "rapid_review_dialog.h"
#include "ui_rapid_review_dialog.h"
#include "private/rapid_review_dialog_p.h"

#include <QtGui/QStandardItemModel>

#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/helper.h>

#include <utils/common/scoped_value_rollback.h>
#include <nx/client/core/utils/human_readable.h>

#include <nx/utils/log/assert.h>

namespace {

/* Font size for info labels. */
const int kInfoFontPixelSize = 16;

QString durationMsToString(qint64 durationMs)
{
    using HumanReadable = nx::client::core::HumanReadable;

    static const QString kSeparator(L' ');

    return HumanReadable::timeSpan(std::chrono::milliseconds(durationMs),
        HumanReadable::DaysAndTime | HumanReadable::Milliseconds,
        HumanReadable::SuffixFormat::Short,
        kSeparator);
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace dialogs {

qint64 ExportRapidReview::kMinimalSourcePeriodLength = ExportRapidReviewPrivate::kMinimalLengthMs
    * ExportRapidReviewPrivate::kMinimalSpeed;

ExportRapidReview::ExportRapidReview(QWidget* parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    d_ptr(new ExportRapidReviewPrivate()),
    ui(new Ui::ExportRapidReviewDialog),
    m_updating(false)
{
    ui->setupUi(this);
    ui->speedSlider->setProperty(style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));

    ui->initialLengthLabel->setForegroundRole(QPalette::Text);
    ui->framesLabel->setForegroundRole(QPalette::Text);

    Q_D(ExportRapidReview);

    setHelpTopic(this, Qn::Rapid_Review_Help);

    QFont infoFont(this->font());
    infoFont.setPixelSize(kInfoFontPixelSize);

    ui->initialLengthLabel->setFont(infoFont);
    ui->framesLabel->setFont(infoFont);

    ui->resultLengthUnitsComboBox->setModel(d->unitsModel);

    connect(ui->speedSpinBox, QnSpinboxIntValueChanged, this,
        [this](int absoluteValue)
        {
            if (m_updating)
                return;

            setSpeed(absoluteValue);
        });

    connect(ui->speedSlider, &QSlider::valueChanged, this,
        [this](int value)
        {
            if (m_updating)
                return;

            QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
            Q_D(ExportRapidReview);
            d->setSliderSpeed(value);
            updateRanges();
            updateControls();
        });

    connect(ui->resultLengthSpinBox, QnSpinboxDoubleValueChanged, this,
        [this](double value)
        {
            if (m_updating)
                return;

            QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
            Q_D(ExportRapidReview);
            d->setResultLengthInSelectedUnits(value);
            updateRanges();
            updateControls();
        });

    connect(ui->resultLengthUnitsComboBox, QnComboboxCurrentIndexChanged, this,
        [this](int index)
        {
            if (m_updating)
                return;

            QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
            Q_D(ExportRapidReview);
            d->setSelectedLengthUnit(index);
            updateRanges();
            updateControls();
        });

    setSourcePeriodLengthMs(kMinimalSourcePeriodLength);
}

ExportRapidReview::~ExportRapidReview()
{
}

void ExportRapidReview::setSpeed(qint64 absoluteValue)
{
    NX_ASSERT(!m_updating, Q_FUNC_INFO,
        "Function belongs to outer interface, must not be called internally.");
    if (m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
    Q_D(ExportRapidReview);
    d->setAbsoluteSpeed(absoluteValue);
    updateRanges();
    updateControls();
}

qint64 ExportRapidReview::speed() const
{
    return ui->speedSpinBox->value();
}

qint64 ExportRapidReview::sourcePeriodLengthMs() const
{
    Q_D(const ExportRapidReview);
    return d->sourcePeriodLengthMs();
}

void ExportRapidReview::setSourcePeriodLengthMs(qint64 lengthMs)
{
    NX_ASSERT(!m_updating, Q_FUNC_INFO,
        "Function belongs to outer interface, must not be called internally.");
    if (m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    Q_D(ExportRapidReview);
    d->setSourcePeriodLengthMs(lengthMs);

    updateRanges();
    updateControls();

    ui->initialLengthLabel->setText(durationMsToString(lengthMs));
}

qint64 ExportRapidReview::frameStepMs() const
{
    Q_D(const ExportRapidReview);
    return d->frameStepMs();
}

void ExportRapidReview::updateRanges()
{
    NX_EXPECT(m_updating);
    Q_D(const ExportRapidReview);

    ui->speedSpinBox->setRange(d->minAbsoluteSpeed(), d->maxAbsoluteSpeed());
    ui->speedSlider->setRange(d->minSliderSpeed(), d->maxSliderSpeed());
    ui->resultLengthSpinBox->setRange(d->minResultLengthInSelectedUnits(),
        d->maxResultLengthInSelectedUnits());
}

void ExportRapidReview::updateControls()
{
    NX_EXPECT(m_updating);
    Q_D(const ExportRapidReview);

    ui->speedSpinBox->setValue(d->absoluteSpeed());
    ui->speedSlider->setValue(d->sliderSpeed());
    ui->resultLengthUnitsComboBox->setCurrentIndex(d->selectedLengthUnitIndex());
    ui->resultLengthSpinBox->setValue(d->resultLengthInSelectedUnits());
    ui->framesLabel->setText(durationMsToString(d->frameStepMs()));
}

} // namespace dialogs
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
