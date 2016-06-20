#include "export_timelapse_dialog.h"
#include "ui_export_timelapse_dialog.h"

#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/scoped_value_rollback.h>
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

QString durationMsToString(qint64 durationMs, const QStandardItemModel* unitsModel)
{
    quint64 duration = durationMs;
    qint64 prevDelimiter = 1;
    QStringList result;

    for (int i = 0; i < unitsModel->rowCount(); ++i)
    {
        QStandardItem* item = unitsModel->item(i);
        qint64 measureUnit = item->data().toLongLong();
        duration /= (measureUnit / prevDelimiter);
        prevDelimiter = measureUnit;

        qint64 value = duration;
        if (i < unitsModel->rowCount() - 1)
        {
            QStandardItem* nextItem = unitsModel->item(i+1);
            qint64 nextMeasureUnit = nextItem->data().toLongLong();
            int upperLimit = nextMeasureUnit / measureUnit;
            value %= upperLimit;
        }
        duration -= value;
        if (value > 0 || !result.isEmpty())
            result.insert(0, QString(lit("%1%2").arg(value).arg(item->text())));
        if (duration == 0)
            break;
    }
    // keep 2 most significant parts only to make string shorter
    result = result.mid(0, 2);
    return result.join(lit(" "));
}

void addModelItem(const QString& text, qint64 measureUnit, QStandardItemModel* model)
{
    QStandardItem* item = new QStandardItem(text);
    item->setData(measureUnit);
    model->appendRow(item);
}

void addModelItem(const QString& text, qint64 measureUnit, QStandardItemModel* model1, QStandardItemModel* model2)
{
    if (model1)
        addModelItem(text, measureUnit, model1);
    if (model2)
        addModelItem(text, measureUnit, model2);
}

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

    m_unitsModelMs = new QStandardItemModel(this);
    m_unitsModelSec = new QStandardItemModel(this);

    addModelItem(tr("ms"), 1, m_unitsModelMs);
    addModelItem(tr("sec"), 1000, m_unitsModelMs, m_unitsModelSec);
    addModelItem(tr("min"), 1000 * 60, m_unitsModelMs, m_unitsModelSec);
    addModelItem(tr("hrs"), 1000 * 60 * 60, m_unitsModelMs, m_unitsModelSec);
    addModelItem(tr("day"), 1000 * 60 * 60 * 24, m_unitsModelMs, m_unitsModelSec);

    ui->resultLengthUnitsComboBox->setModel(m_unitsModelSec);

    connect(ui->speedSpinBox, QnSpinboxIntValueChanged, this, [this](int value)
    {
        if (m_updating)
            return;

        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        ui->speedSlider->setValue(value);
        setExpectedLengthMs(m_sourcePeriodLengthMs / value);
    });

    connect(ui->speedSlider, &QSlider::valueChanged, this, [this](int value)
    {
        if (m_updating)
            return;

        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        ui->speedSpinBox->setValue(value);
        setExpectedLengthMs(m_sourcePeriodLengthMs / value);
    });

    auto expectedLengthChanged = [this]()
    {
        if (m_updating)
            return;

        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

        int index = ui->resultLengthUnitsComboBox->currentIndex();
        qint64 measureUnit = m_unitsModelSec->item(index)->data().toLongLong();
        qint64 speedValue = kMaximalSpeed;
        if (ui->resultLengthSpinBox->value() > 0)
            speedValue = m_sourcePeriodLengthMs / (ui->resultLengthSpinBox->value() * measureUnit);
        ui->speedSlider->setValue(speedValue);
        ui->speedSpinBox->setValue(speedValue);

        updateFrameStep(speedValue);
    };

    connect(ui->resultLengthSpinBox, QnSpinboxIntValueChanged, this, expectedLengthChanged);
    connect(ui->resultLengthUnitsComboBox, QnComboboxCurrentIndexChanged, this, expectedLengthChanged);


    initControls();
}

void QnExportTimelapseDialog::setSpeed(qint64 value)
{
    ui->speedSpinBox->setValue(value);
    ui->speedSlider->setValue(value);
    setExpectedLengthMs(m_sourcePeriodLengthMs / value);
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

    int maxSpeed = m_sourcePeriodLengthMs / kMinimalLengthMs;
    ui->speedSpinBox->setRange(kMinimalSpeed, maxSpeed);
    ui->speedSlider->setRange(kMinimalSpeed, maxSpeed);
    //Q_ASSERT(toSliderScale(kMinimalSpeed) == 0);

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
    ui->speedSlider->setValue(speed);
    setExpectedLengthMs(expectedLengthMs);
    ui->initialLengthLabel->setText(durationMsToString(m_sourcePeriodLengthMs, m_unitsModelSec));
}

void QnExportTimelapseDialog::setExpectedLengthMs(qint64 value)
{
    int speed = m_sourcePeriodLengthMs / value;
    updateFrameStep(speed);
    int index = ui->resultLengthUnitsComboBox->currentIndex();
    qint64 measureUnit = m_unitsModelSec->item(index)->data().toLongLong();
    ui->resultLengthSpinBox->setValue(static_cast<int>(value / measureUnit));
}

void QnExportTimelapseDialog::updateFrameStep(int speed)
{
    m_expectedLengthMs = speed;
    m_frameStepMs = speed *  ( 1000 / kResultFps);
    ui->framesLabel->setText(durationMsToString(m_frameStepMs, m_unitsModelMs));
}
