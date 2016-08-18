#include "look_and_feel_preferences_widget.h"
#include "ui_look_and_feel_preferences_widget.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtGui/QDesktopServices>

#include <client/client_settings.h>
#include <client/client_globals.h>
#include <client/client_translation_manager.h>

#include <common/common_module.h>

#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_dialog.h>
#include <ui/dialogs/common/progress_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/translation_list_model.h>
#include <ui/style/custom_style.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_auto_starter.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/app_info.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/local_file_cache.h>

QnLookAndFeelPreferencesWidget::QnLookAndFeelPreferencesWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::LookAndFeelPreferencesWidget),
    m_updating(false),
    m_oldLanguage(0),
    m_oldTimeMode(Qn::ServerTimeMode)
{
    ui->setupUi(this);

    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(this);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());

    ui->timeModeWarningLabel->setText(tr("This option will not affect Recording Schedule. Recording Schedule is always based on Server Time."));

    setHelpTopic(this,                                                        Qn::SystemSettings_General_Customizing_Help);
    setHelpTopic(ui->languageLabel,           ui->languageComboBox,           Qn::SystemSettings_General_Language_Help);
    setHelpTopic(ui->tourCycleTimeLabel,      ui->tourCycleTimeSpinBox,       Qn::SystemSettings_General_TourCycleTime_Help);
    setHelpTopic(ui->showIpInTreeCheckBox,                                    Qn::SystemSettings_General_ShowIpInTree_Help);

    setupLanguageUi();
    setupTimeModeUi();
    setupBackgroundUi();
}

QnLookAndFeelPreferencesWidget::~QnLookAndFeelPreferencesWidget()
{
}

void QnLookAndFeelPreferencesWidget::applyChanges()
{
    qnSettings->setTourCycleTime(ui->tourCycleTimeSpinBox->value() * 1000);
    qnSettings->setExtraInfoInTree(
        ui->showIpInTreeCheckBox->isChecked()
        ? Qn::RI_FullInfo
        : Qn::RI_NameOnly
    );
    qnSettings->setTimeMode(static_cast<Qn::TimeMode>(ui->timeModeComboBox->itemData(ui->timeModeComboBox->currentIndex()).toInt()));

    QnTranslation translation = ui->languageComboBox->itemData(ui->languageComboBox->currentIndex(), Qn::TranslationRole).value<QnTranslation>();
    if (!translation.isEmpty())
    {
        if (!translation.filePaths().isEmpty())
        {
            QString currentTranslationPath = qnSettings->translationPath();
            if (!translation.filePaths().contains(currentTranslationPath))
                qnSettings->setTranslationPath(translation.filePaths()[0]);
        }
    }
}

void QnLookAndFeelPreferencesWidget::loadDataToUi()
{
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    ui->tourCycleTimeSpinBox->setValue(qnSettings->tourCycleTime() / 1000);
    ui->showIpInTreeCheckBox->setChecked(qnSettings->extraInfoInTree() != Qn::RI_NameOnly);

    ui->timeModeComboBox->setCurrentIndex(ui->timeModeComboBox->findData(qnSettings->timeMode()));

    m_oldLanguage = -1;
    int defaultLanguageIndex = -1;
    QString translationPath = qnSettings->translationPath();
    for(int i = 0; i < ui->languageComboBox->count(); i++)
    {
        QnTranslation translation = ui->languageComboBox->itemData(i, Qn::TranslationRole).value<QnTranslation>();
        if(translation.filePaths().contains(translationPath)) {
            m_oldLanguage = i;
            break;
        }

        if (translation.localeCode() == QnAppInfo::defaultLanguage())
            defaultLanguageIndex = i;
    }

    if (m_oldLanguage < 0) {
        NX_ASSERT(defaultLanguageIndex >= 0, Q_FUNC_INFO, "default language must definitely be present in translations");
        m_oldLanguage = std::max(defaultLanguageIndex, 0);
    }

    ui->languageComboBox->setCurrentIndex(m_oldLanguage);

    bool backgroundAllowed = !(qnSettings->lightMode() & Qn::LightModeNoSceneBackground);
    ui->imageGroupBox->setEnabled(backgroundAllowed);

    QnBackgroundImage background = qnSettings->backgroundImage();
    m_oldBackground = background;

    if (!backgroundAllowed)
    {
        ui->imageGroupBox->setChecked(false);
    }
    else
    {
        ui->imageGroupBox->setChecked(background.enabled);
        ui->imageNameLineEdit->setText(background.originalName);
        ui->imageModeComboBox->setCurrentIndex(ui->imageModeComboBox->findData(qVariantFromValue(background.mode)));
        ui->imageOpacitySpinBox->setValue(qRound(background.opacity * 100));
    }
}

bool QnLookAndFeelPreferencesWidget::hasChanges() const
{
    //TODO: #GDM implement me
    return true;
}


bool QnLookAndFeelPreferencesWidget::isRestartRequired() const
{
    /* These changes can be applied only after client restart. */
    return m_oldLanguage != ui->languageComboBox->currentIndex();
}

bool QnLookAndFeelPreferencesWidget::canDiscardChanges() const
{
    //TODO: #GDM restoring changes does not belongs here
    bool backgroundAllowed = !(qnSettings->lightMode() & Qn::LightModeNoSceneBackground);
    if (backgroundAllowed)
        qnSettings->setBackgroundImage(m_oldBackground);
    return true;
}

void QnLookAndFeelPreferencesWidget::selectBackgroundImage()
{
    QString nameFilter;
    foreach (const QByteArray &format, QImageReader::supportedImageFormats()) {
        if (!nameFilter.isEmpty())
            nameFilter += QLatin1Char(' ');
        nameFilter += QLatin1String("*.") + QLatin1String(format);
    }
    nameFilter = QLatin1Char('(') + nameFilter + QLatin1Char(')');

    const QString previousName = qnSettings->backgroundImage().name;
    QString folder = QFileInfo(previousName).absolutePath();
    if (folder.isEmpty())
        folder = qnSettings->backgroundsFolder();

    QScopedPointer<QnCustomFileDialog> dialog(
        new QnCustomFileDialog(
            this, tr("Select File..."),
            folder,
            tr("Pictures %1").arg(nameFilter)
        )
    );
    dialog->setFileMode(QFileDialog::ExistingFile);

    if(!dialog->exec())
        return;

    QString originalFileName = dialog->selectedFile();
    if (originalFileName.isEmpty())
        return;

    qnSettings->setBackgroundsFolder(QFileInfo(originalFileName).absolutePath());

    QString cachedName = QnAppServerImageCache::cachedImageFilename(originalFileName);
    if (previousName == cachedName ||
        QDir::toNativeSeparators(previousName).toLower() ==
        QDir::toNativeSeparators(originalFileName).toLower())
        return;

    QnProgressDialog* progressDialog = new QnProgressDialog(this);
    progressDialog->setWindowTitle(tr("Preparing Image..."));
    progressDialog->setLabelText(tr("Please wait while image is being prepared..."));
    progressDialog->setInfiniteProgress();
    progressDialog->setModal(true);

    QnLocalFileCache* imgCache = new QnLocalFileCache(this);
    connect(imgCache, &QnAppServerFileCache::fileUploaded, this, [this, imgCache, progressDialog, originalFileName](const QString &storedFileName) {
        if (!progressDialog->wasCanceled()) {
            QnBackgroundImage background = qnSettings->backgroundImage();
            background.name = storedFileName;
            background.originalName = QFileInfo(originalFileName).fileName();
            qnSettings->setBackgroundImage(background);
            ui->imageNameLineEdit->setText( background.originalName);
        }
        imgCache->deleteLater();
        progressDialog->hide();
        progressDialog->deleteLater();
    });

    imgCache->storeImage(originalFileName);
    progressDialog->exec();
}

void QnLookAndFeelPreferencesWidget::setupLanguageUi()
{
    QnTranslationListModel *model = new QnTranslationListModel(this);
    model->setTranslations(qnCommon->instance<QnClientTranslationManager>()->loadTranslations());
    ui->languageComboBox->setModel(model);

    setWarningStyle(ui->languageWarningLabel);
    ui->languageWarningLabel->setVisible(false);

    connect(ui->languageComboBox, QnComboboxCurrentIndexChanged,  this,   [this](int index) {
        ui->languageWarningLabel->setVisible(m_oldLanguage != index);
    });
}

void QnLookAndFeelPreferencesWidget::setupTimeModeUi()
{
    ui->timeModeComboBox->addItem(tr("Server Time"), Qn::ServerTimeMode);
    ui->timeModeComboBox->addItem(tr("Client Time"), Qn::ClientTimeMode);
    connect(ui->timeModeComboBox, QnComboboxActivated,  this,   [this](int index)
    {
        Qn::TimeMode selectedMode = static_cast<Qn::TimeMode>(ui->timeModeComboBox->itemData(index).toInt());
        ui->timeModeWarningLabel->setVisible(m_oldTimeMode == Qn::ServerTimeMode && selectedMode == Qn::ClientTimeMode);
    });
    setWarningStyle(ui->timeModeWarningLabel);
    ui->timeModeWarningLabel->setVisible(false);
}

void QnLookAndFeelPreferencesWidget::setupBackgroundUi()
{
    ui->imageModeComboBox->addItem(tr("Stretch"), qVariantFromValue(Qn::ImageBehaviour::Stretch));
    ui->imageModeComboBox->addItem(tr("Fit"),     qVariantFromValue(Qn::ImageBehaviour::Fit));
    ui->imageModeComboBox->addItem(tr("Crop"),    qVariantFromValue(Qn::ImageBehaviour::Crop));

    connect(ui->imageGroupBox, &QGroupBox::toggled, this, [this] (bool checked) {
        if (m_updating)
            return;

        QnBackgroundImage background = qnSettings->backgroundImage();
        background.enabled = checked;
        qnSettings->setBackgroundImage(background);
    });

    connect(ui->imageSelectButton,        &QPushButton::clicked,    this,   &QnLookAndFeelPreferencesWidget::selectBackgroundImage);
    connect(ui->imageModeComboBox,        QnComboboxCurrentIndexChanged,  this,   [this]
    {
        if (m_updating)
            return;

        QnBackgroundImage background = qnSettings->backgroundImage();
        background.mode = ui->imageModeComboBox->currentData().value<Qn::ImageBehaviour>();
        qnSettings->setBackgroundImage(background);
    });

    connect(ui->imageOpacitySpinBox,      QnSpinboxIntValueChanged, this,   [this](int value)
    {
        if (m_updating)
            return;

        QnBackgroundImage background = qnSettings->backgroundImage();
        background.opacity = 0.01 * value;
        qnSettings->setBackgroundImage(background);
    });
}
