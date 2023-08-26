// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "look_and_feel_preferences_widget.h"
#include "ui_look_and_feel_preferences_widget.h"

#include <QtCore/QDir>
#include <QtCore/QScopedValueRollback>

#include <client/client_globals.h>
#include <client/client_runtime_settings.h>
#include <nx/branding.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/dialogs/progress_dialog.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/utils/local_file_cache.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/models/translation_list_model.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client::desktop;

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

    ui->imageNameLineEdit->setPlaceholderText('<' + tr("No image") + '>');

    setHelpTopic(this,                                                        HelpTopic::Id::SystemSettings_General_Customizing);
    setHelpTopic(ui->tourCycleTimeLabel,      ui->tourCycleTimeSpinBox,       HelpTopic::Id::SystemSettings_General_TourCycleTime);
    setHelpTopic(ui->showIpInTreeCheckBox,                                    HelpTopic::Id::SystemSettings_General_ShowIpInTree);

    ui->showIpInTreeCheckBoxHint->setHintText(
        tr("Displays IP addresses for cameras and servers and roles for users."));

    ui->timeModeLabel->setHint(tr("Sets the time to use in the client (timeline, timestamps, etc.) when client and server machines are in different time zones."));
    ui->tourCycleTimeLabel->setHint(tr("Length of time each camera on layout will be displayed if you start a Tour."));
    setHelpTopic(ui->tourCycleTimeLabel, HelpTopic::Id::SystemSettings_General_TourCycleTime);

    auto aligner = new Aligner(this);
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

    connect(ui->tourCycleTimeSpinBox, QnSpinboxIntValueChanged,
        this, &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->showIpInTreeCheckBox, &QCheckBox::clicked,
        this, &QnAbstractPreferencesWidget::hasChangesChanged);

    connect(ui->usePtzAimOverlayCheckBox, &QCheckBox::clicked,
        this, &QnAbstractPreferencesWidget::hasChangesChanged);
}

QnLookAndFeelPreferencesWidget::~QnLookAndFeelPreferencesWidget()
{
}

void QnLookAndFeelPreferencesWidget::applyChanges()
{
    appContext()->localSettings()->tourCycleTimeMs = selectedTourCycleTimeMs();
    appContext()->localSettings()->resourceInfoLevel = selectedInfoLevel();
    appContext()->localSettings()->timeMode = selectedTimeMode();
    appContext()->localSettings()->locale = selectedTranslation();
    appContext()->localSettings()->ptzAimOverlayEnabled = isPtzAimOverlayEnabled();

    /* Background changes are applied instantly. */
    if (backgroundAllowed())
        m_oldBackground = appContext()->localSettings()->backgroundImage();
}

void QnLookAndFeelPreferencesWidget::loadDataToUi()
{
    QScopedValueRollback<bool> guard(m_updating, true);

    ui->tourCycleTimeSpinBox->setValue(appContext()->localSettings()->tourCycleTimeMs() / 1000);
    ui->showIpInTreeCheckBox->setChecked(appContext()->localSettings()->resourceInfoLevel() != Qn::RI_NameOnly);

    ui->timeModeComboBox->setCurrentIndex(
        ui->timeModeComboBox->findData(appContext()->localSettings()->timeMode()));

    ui->usePtzAimOverlayCheckBox->setChecked(
        appContext()->localSettings()->ptzAimOverlayEnabled());

    int defaultLanguageIndex = -1;
    int currentLanguage = -1;
    QString locale = appContext()->localSettings()->locale();
    for (int i = 0; i < ui->languageComboBox->count(); i++)
    {
        const auto translation = ui->languageComboBox->itemData(
            i, QnTranslationListModel::TranslationRole).value<TranslationInfo>();

        if (translation.localeCode == locale)
            currentLanguage = i;

        if (translation.localeCode == nx::branding::defaultLocale())
            defaultLanguageIndex = i;
    }

    if (currentLanguage < 0)
    {
        NX_ASSERT(defaultLanguageIndex >= 0, "default language must definitely be present in translations");
        currentLanguage = std::max(defaultLanguageIndex, 0);
    }

    ui->languageComboBox->setCurrentIndex(currentLanguage);

    ui->imageGroupBox->setEnabled(backgroundAllowed());

    BackgroundImage background = appContext()->localSettings()->backgroundImage();
    m_oldBackground = background;

    if (!backgroundAllowed())
    {
        ui->imageGroupBox->setChecked(false);
    }
    else
    {
        ui->imageGroupBox->setChecked(background.enabled);
        ui->imageNameLineEdit->setText(background.originalName);
        ui->imageModeComboBox->setCurrentIndex(ui->imageModeComboBox->findData(QVariant::fromValue(background.mode)));
        ui->imageOpacitySpinBox->setValue(opacityToPercent(background.opacity));
    }
}

bool QnLookAndFeelPreferencesWidget::hasChanges() const
{
    /* Background changes are applied instantly. */
    if (backgroundAllowed() && appContext()->localSettings()->backgroundImage() != m_oldBackground)
        return true;

    return appContext()->localSettings()->locale() != selectedTranslation()
        || appContext()->localSettings()->timeMode() != selectedTimeMode()
        || appContext()->localSettings()->resourceInfoLevel() != selectedInfoLevel()
        || appContext()->localSettings()->tourCycleTimeMs() != selectedTourCycleTimeMs()
        || appContext()->localSettings()->ptzAimOverlayEnabled() != isPtzAimOverlayEnabled();
}


void QnLookAndFeelPreferencesWidget::discardChanges()
{
    if (backgroundAllowed())
        appContext()->localSettings()->backgroundImage = m_oldBackground;
}

bool QnLookAndFeelPreferencesWidget::isRestartRequired() const
{
    /* These changes can be applied only after client restart. */
    return qnRuntime->locale() != selectedTranslation();
}

void QnLookAndFeelPreferencesWidget::selectBackgroundImage()
{
    QString prevOriginalName = appContext()->localSettings()->backgroundImage().originalName;
    QString folder = QFileInfo(prevOriginalName).absolutePath();
    if (folder.isEmpty())
        folder = appContext()->localSettings()->backgroundsFolder();

    const QString previousCachedName = appContext()->localSettings()->backgroundImage().name;

    QScopedPointer<QnCustomFileDialog> dialog(
        new QnCustomFileDialog(
            this, tr("Select File..."),
            folder,
            QnCustomFileDialog::createFilter(QnCustomFileDialog::picturesFilter())
        )
    );
    dialog->setFileMode(QFileDialog::ExistingFile);

    if (!dialog->exec())
        return;

    QString originalFileName = dialog->selectedFile();
    if (originalFileName.isEmpty())
        return;

    appContext()->localSettings()->backgroundsFolder = QFileInfo(originalFileName).absolutePath();

    QString cachedName = ServerImageCache::cachedImageFilename(originalFileName);
    if (previousCachedName == cachedName ||
        QDir::toNativeSeparators(previousCachedName).toLower() ==
        QDir::toNativeSeparators(originalFileName).toLower())
        return;

    auto progressDialog = new ProgressDialog(this);
    progressDialog->setWindowTitle(tr("Preparing Image..."));
    progressDialog->setText(tr("Please wait while image is being prepared..."));
    progressDialog->setInfiniteMode();
    progressDialog->setModal(true);

    auto imgCache = new LocalFileCache(this);
    connect(imgCache, &ServerFileCache::fileUploaded, this,
        [this, imgCache, progressDialog, originalFileName](const QString &storedFileName)
        {
            if (!progressDialog->wasCanceled())
            {
                BackgroundImage background = appContext()->localSettings()->backgroundImage();
                background.name = storedFileName;
                background.originalName = originalFileName;
                appContext()->localSettings()->backgroundImage = background;
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
    return !qnRuntime->lightMode().testFlag(Qn::LightModeNoSceneBackground);
}

QString QnLookAndFeelPreferencesWidget::selectedTranslation() const
{
    const auto translation = ui->languageComboBox->itemData(
        ui->languageComboBox->currentIndex(), QnTranslationListModel::TranslationRole)
        .value<TranslationInfo>();

    return translation.localeCode;
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

bool QnLookAndFeelPreferencesWidget::isPtzAimOverlayEnabled() const
{
    return ui->usePtzAimOverlayCheckBox->isChecked();
}

int QnLookAndFeelPreferencesWidget::selectedTourCycleTimeMs() const
{
    return ui->tourCycleTimeSpinBox->value() * 1000;
}

Qn::ImageBehavior QnLookAndFeelPreferencesWidget::selectedImageMode() const
{
    return ui->imageModeComboBox->currentData().value<Qn::ImageBehavior>();
}

void QnLookAndFeelPreferencesWidget::setupLanguageUi()
{
    ui->languageComboBox->setModel(new QnTranslationListModel(this));

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
            ui->timeModeWarningLabel->setVisible(
                appContext()->localSettings()->timeMode() == Qn::ServerTimeMode
                    && selectedTimeMode() == Qn::ClientTimeMode);
            emit hasChangesChanged();
        });
    setWarningStyle(ui->timeModeWarningLabel);
    ui->timeModeWarningLabel->setVisible(false);
}

void QnLookAndFeelPreferencesWidget::setupBackgroundUi()
{
    ui->imageModeComboBox->addItem(tr("Stretch"), QVariant::fromValue(Qn::ImageBehavior::Stretch));
    ui->imageModeComboBox->addItem(tr("Fit"),     QVariant::fromValue(Qn::ImageBehavior::Fit));
    ui->imageModeComboBox->addItem(tr("Crop"),    QVariant::fromValue(Qn::ImageBehavior::Crop));

    connect(ui->imageGroupBox, &QGroupBox::toggled, this,
        [this](bool checked)
        {
            if (m_updating)
                return;

            BackgroundImage background = appContext()->localSettings()->backgroundImage();
            background.enabled = checked;
            appContext()->localSettings()->backgroundImage = background;
            emit hasChangesChanged();
        });

    connect(ui->imageSelectButton, &QPushButton::clicked, this,
        &QnLookAndFeelPreferencesWidget::selectBackgroundImage);

    connect(ui->imageModeComboBox, QnComboboxCurrentIndexChanged, this,
        [this]
        {
            if (m_updating)
                return;

            BackgroundImage background = appContext()->localSettings()->backgroundImage();
            background.mode = selectedImageMode();
            appContext()->localSettings()->backgroundImage = background;
            emit hasChangesChanged();
        });

    connect(ui->imageOpacitySpinBox, QnSpinboxIntValueChanged, this,
        [this](int value)
        {
            if (m_updating)
                return;

            BackgroundImage background = appContext()->localSettings()->backgroundImage();
            background.opacity = opacityFromPercent(value);
            appContext()->localSettings()->backgroundImage = background;
            emit hasChangesChanged();
        });
}
