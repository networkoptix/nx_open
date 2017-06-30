#include "look_and_feel_preferences_widget.h"
#include "ui_look_and_feel_preferences_widget.h"

#include <QtCore/QDir>
#include <QtGui/QImageReader>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_globals.h>
#include <client/client_translation_manager.h>

#include <common/common_module.h>

#include <ui/common/aligner.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_dialog.h>
#include <ui/dialogs/common/progress_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/translation_list_model.h>
#include <ui/style/custom_style.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/app_info.h>
#include <utils/common/scoped_value_rollback.h>
#include <nx/client/desktop/utils/local_file_cache.h>

using namespace nx::client::desktop;

namespace {

qreal opacityFromPercent(int percent)
{
    return 0.01 * percent;
}

int opacityToPercent(qreal opacity)
{
    return qRound(opacity * 100);
}

} // namespace

QnLookAndFeelPreferencesWidget::QnLookAndFeelPreferencesWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::LookAndFeelPreferencesWidget)
{
    ui->setupUi(this);

    ui->timeModeWarningLabel->setText(
        tr("This option will not affect Recording Schedule. "
           "Recording Schedule is always based on Server Time."));

    ui->imageNameLineEdit->setPlaceholderText(L'<' + tr("No image") + L'>');

    setHelpTopic(this,                                                        Qn::SystemSettings_General_Customizing_Help);
    setHelpTopic(ui->languageLabel,           ui->languageComboBox,           Qn::SystemSettings_General_Language_Help);
    setHelpTopic(ui->tourCycleTimeLabel,      ui->tourCycleTimeSpinBox,       Qn::SystemSettings_General_TourCycleTime_Help);
    setHelpTopic(ui->showIpInTreeCheckBox,                                    Qn::SystemSettings_General_ShowIpInTree_Help);

    auto aligner = new QnAligner(this);
    aligner->addWidgets({
        ui->languageLabel,
        ui->timeModeLabel,
        ui->tourCycleTimeLabel,
        ui->imageNameLabel,
        ui->imageModeLabel,
        ui->imageOpacityLabel
    });

    setupLanguageUi();
    setupTimeModeUi();
    setupBackgroundUi();

    connect(ui->tourCycleTimeSpinBox, QnSpinboxIntValueChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
    connect(ui->showIpInTreeCheckBox, &QCheckBox::clicked, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
}

QnLookAndFeelPreferencesWidget::~QnLookAndFeelPreferencesWidget()
{
}

void QnLookAndFeelPreferencesWidget::applyChanges()
{
    qnSettings->setTourCycleTime(selectedTourCycleTimeMs());
    qnSettings->setExtraInfoInTree(selectedInfoLevel());
    qnSettings->setTimeMode(selectedTimeMode());

    //TODO: #GDM store locale code instead
    qnSettings->setTranslationPath(selectedTranslation());

    /* Background changes are applied instantly. */
    if (backgroundAllowed())
        m_oldBackground = qnSettings->backgroundImage();
}

void QnLookAndFeelPreferencesWidget::loadDataToUi()
{
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    ui->tourCycleTimeSpinBox->setValue(qnSettings->tourCycleTime() / 1000);
    ui->showIpInTreeCheckBox->setChecked(qnSettings->extraInfoInTree() != Qn::RI_NameOnly);

    ui->timeModeComboBox->setCurrentIndex(ui->timeModeComboBox->findData(qnSettings->timeMode()));

    int defaultLanguageIndex = -1;
    int currentLanguage = -1;
    QString translationPath = qnSettings->translationPath();
    for (int i = 0; i < ui->languageComboBox->count(); i++)
    {
        QnTranslation translation = ui->languageComboBox->itemData(
            i, Qn::TranslationRole).value<QnTranslation>();

        if (translation.filePaths().contains(translationPath))
            currentLanguage = i;

        if (translation.localeCode() == QnAppInfo::defaultLanguage())
            defaultLanguageIndex = i;
    }

    if (currentLanguage < 0)
    {
        NX_ASSERT(defaultLanguageIndex >= 0, Q_FUNC_INFO, "default language must definitely be present in translations");
        currentLanguage = std::max(defaultLanguageIndex, 0);
    }

    ui->languageComboBox->setCurrentIndex(currentLanguage);

    ui->imageGroupBox->setEnabled(backgroundAllowed());

    QnBackgroundImage background = qnSettings->backgroundImage();
    m_oldBackground = background;

    if (!backgroundAllowed())
    {
        ui->imageGroupBox->setChecked(false);
    }
    else
    {
        ui->imageGroupBox->setChecked(background.enabled);
        ui->imageNameLineEdit->setText(background.originalName);
        ui->imageModeComboBox->setCurrentIndex(ui->imageModeComboBox->findData(qVariantFromValue(background.mode)));
        ui->imageOpacitySpinBox->setValue(opacityToPercent(background.opacity));
    }
}

bool QnLookAndFeelPreferencesWidget::hasChanges() const
{
    /* Background changes are applied instantly. */
    if (backgroundAllowed() && qnSettings->backgroundImage() != m_oldBackground)
        return true;

    return qnSettings->translationPath() != selectedTranslation()
        || qnSettings->timeMode() != selectedTimeMode()
        || qnSettings->extraInfoInTree() != selectedInfoLevel()
        || qnSettings->tourCycleTime() != selectedTourCycleTimeMs();
}


void QnLookAndFeelPreferencesWidget::discardChanges()
{
    if (backgroundAllowed())
        qnSettings->setBackgroundImage(m_oldBackground);
}

bool QnLookAndFeelPreferencesWidget::isRestartRequired() const
{
    /* These changes can be applied only after client restart. */
    return qnRuntime->translationPath() != selectedTranslation();
}

void QnLookAndFeelPreferencesWidget::selectBackgroundImage()
{
    QString nameFilter;
    for (const QByteArray&format: QImageReader::supportedImageFormats())
    {
        if (!nameFilter.isEmpty())
            nameFilter += L' ';
        nameFilter += lit("*.") + QLatin1String(format);
    }
    nameFilter = L'(' + nameFilter + L')';

    QString prevOriginalName = qnSettings->backgroundImage().originalName;
    QString folder = QFileInfo(prevOriginalName).absolutePath();
    if (folder.isEmpty())
        folder = qnSettings->backgroundsFolder();

    const QString previousCachedName = qnSettings->backgroundImage().name;

    QScopedPointer<QnCustomFileDialog> dialog(
        new QnCustomFileDialog(
            this, tr("Select File..."),
            folder,
            tr("Pictures %1").arg(nameFilter)
        )
    );
    dialog->setFileMode(QFileDialog::ExistingFile);

    if (!dialog->exec())
        return;

    QString originalFileName = dialog->selectedFile();
    if (originalFileName.isEmpty())
        return;

    qnSettings->setBackgroundsFolder(QFileInfo(originalFileName).absolutePath());

    QString cachedName = ServerImageCache::cachedImageFilename(originalFileName);
    if (previousCachedName == cachedName ||
        QDir::toNativeSeparators(previousCachedName).toLower() ==
        QDir::toNativeSeparators(originalFileName).toLower())
        return;

    auto progressDialog = new QnProgressDialog(this);
    progressDialog->setWindowTitle(tr("Preparing Image..."));
    progressDialog->setLabelText(tr("Please wait while image is being prepared..."));
    progressDialog->setInfiniteProgress();
    progressDialog->setModal(true);

    auto imgCache = new LocalFileCache(this);
    connect(imgCache, &ServerFileCache::fileUploaded, this,
        [this, imgCache, progressDialog, originalFileName](const QString &storedFileName)
        {
            if (!progressDialog->wasCanceled())
            {
                QnBackgroundImage background = qnSettings->backgroundImage();
                background.name = storedFileName;
                background.originalName = originalFileName;
                qnSettings->setBackgroundImage(background);
                ui->imageNameLineEdit->setText(QFileInfo(originalFileName).fileName());
                emit hasChangesChanged();
            }
            imgCache->deleteLater();
            progressDialog->hide();
            progressDialog->deleteLater();
        });

    imgCache->storeImage(originalFileName);
    progressDialog->exec();
}

bool QnLookAndFeelPreferencesWidget::backgroundAllowed() const
{
    return !qnSettings->lightMode().testFlag(Qn::LightModeNoSceneBackground);
}

QString QnLookAndFeelPreferencesWidget::selectedTranslation() const
{
    QnTranslation translation = ui->languageComboBox->itemData(
        ui->languageComboBox->currentIndex(), Qn::TranslationRole).value<QnTranslation>();
    return translation.isEmpty()
        ? QString()
        : translation.filePaths()[0];
}

Qn::TimeMode QnLookAndFeelPreferencesWidget::selectedTimeMode() const
{
    return static_cast<Qn::TimeMode>(ui->timeModeComboBox->itemData(
        ui->timeModeComboBox->currentIndex()).toInt());
}

Qn::ResourceInfoLevel QnLookAndFeelPreferencesWidget::selectedInfoLevel() const
{
    return ui->showIpInTreeCheckBox->isChecked()
        ? Qn::RI_FullInfo
        : Qn::RI_NameOnly;
}

int QnLookAndFeelPreferencesWidget::selectedTourCycleTimeMs() const
{
    return ui->tourCycleTimeSpinBox->value() * 1000;
}

Qn::ImageBehaviour QnLookAndFeelPreferencesWidget::selectedImageMode() const
{
    return ui->imageModeComboBox->currentData().value<Qn::ImageBehaviour>();
}

void QnLookAndFeelPreferencesWidget::setupLanguageUi()
{
    auto model = new QnTranslationListModel(this);
    model->setTranslations(commonModule()->instance<QnClientTranslationManager>()->loadTranslations());
    ui->languageComboBox->setModel(model);

    connect(ui->languageComboBox, QnComboboxCurrentIndexChanged, this,
        &QnAbstractPreferencesWidget::hasChangesChanged);
}

void QnLookAndFeelPreferencesWidget::setupTimeModeUi()
{
    ui->timeModeComboBox->addItem(tr("Server Time"), Qn::ServerTimeMode);
    ui->timeModeComboBox->addItem(tr("Client Time"), Qn::ClientTimeMode);
    connect(ui->timeModeComboBox, QnComboboxActivated, this,
        [this]
        {
            ui->timeModeWarningLabel->setVisible(qnSettings->timeMode() == Qn::ServerTimeMode
                && selectedTimeMode() == Qn::ClientTimeMode);
            emit hasChangesChanged();
        });
    setWarningStyle(ui->timeModeWarningLabel);
    ui->timeModeWarningLabel->setVisible(false);
}

void QnLookAndFeelPreferencesWidget::setupBackgroundUi()
{
    ui->imageModeComboBox->addItem(tr("Stretch"), qVariantFromValue(Qn::ImageBehaviour::Stretch));
    ui->imageModeComboBox->addItem(tr("Fit"),     qVariantFromValue(Qn::ImageBehaviour::Fit));
    ui->imageModeComboBox->addItem(tr("Crop"),    qVariantFromValue(Qn::ImageBehaviour::Crop));

    connect(ui->imageGroupBox, &QGroupBox::toggled, this,
        [this](bool checked)
        {
            if (m_updating)
                return;

            QnBackgroundImage background = qnSettings->backgroundImage();
            background.enabled = checked;
            qnSettings->setBackgroundImage(background);
            emit hasChangesChanged();
        });

    connect(ui->imageSelectButton, &QPushButton::clicked, this,
        &QnLookAndFeelPreferencesWidget::selectBackgroundImage);

    connect(ui->imageModeComboBox, QnComboboxCurrentIndexChanged, this,
        [this]
        {
            if (m_updating)
                return;

            QnBackgroundImage background = qnSettings->backgroundImage();
            background.mode = selectedImageMode();
            qnSettings->setBackgroundImage(background);
            emit hasChangesChanged();
        });

    connect(ui->imageOpacitySpinBox, QnSpinboxIntValueChanged, this,
        [this](int value)
        {
            if (m_updating)
                return;

            QnBackgroundImage background = qnSettings->backgroundImage();
            background.opacity = opacityFromPercent(value);
            qnSettings->setBackgroundImage(background);
            emit hasChangesChanged();
        });
}
