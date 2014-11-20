#include "look_and_feel_preferences_widget.h"
#include "ui_look_and_feel_preferences_widget.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QColorDialog>

#include <client/client_settings.h>
#include <client/client_globals.h>
#include <client/client_translation_manager.h>

#include <common/common_module.h>

#include <translation/translation_list_model.h>

#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/file_dialog.h>
#include <ui/dialogs/progress_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/warning_style.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_auto_starter.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/scoped_value_rollback.h>
#include <utils/math/color_transformations.h>
#include <utils/local_file_cache.h>

QnLookAndFeelPreferencesWidget::QnLookAndFeelPreferencesWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::LookAndFeelPreferencesWidget),
    m_colorDialog(new QColorDialog()),
    m_updating(false),
    m_oldLanguage(0),
    m_oldSkin(0),
    m_oldTimeMode(Qn::ServerTimeMode)
{
    ui->setupUi(this);

    setHelpTopic(ui->lookAndFeelGroupBox,                                     Qn::SystemSettings_General_Customizing_Help);
    setHelpTopic(ui->languageLabel,           ui->languageComboBox,           Qn::SystemSettings_General_Language_Help);
    setHelpTopic(ui->tourCycleTimeLabel,      ui->tourCycleTimeSpinBox,       Qn::SystemSettings_General_TourCycleTime_Help);
    setHelpTopic(ui->showIpInTreeLabel,       ui->showIpInTreeCheckBox,       Qn::SystemSettings_General_ShowIpInTree_Help);   

    setupLanguageUi();
    setupSkinUi();
    setupTimeModeUi();
    setupBackgroundUi();
}

QnLookAndFeelPreferencesWidget::~QnLookAndFeelPreferencesWidget()
{
}

bool QnLookAndFeelPreferencesWidget::event(QEvent *event) {
    bool result = base_type::event(event);
    return result;
}

void QnLookAndFeelPreferencesWidget::submitToSettings() {
    qnSettings->setTourCycleTime(ui->tourCycleTimeSpinBox->value() * 1000);
    qnSettings->setIpShownInTree(ui->showIpInTreeCheckBox->isChecked());
    qnSettings->setTimeMode(static_cast<Qn::TimeMode>(ui->timeModeComboBox->itemData(ui->timeModeComboBox->currentIndex()).toInt()));
    qnSettings->setClientSkin(static_cast<Qn::ClientSkin>(ui->skinComboBox->itemData(ui->skinComboBox->currentIndex()).toInt()));

    QnTranslation translation = ui->languageComboBox->itemData(ui->languageComboBox->currentIndex(), Qn::TranslationRole).value<QnTranslation>();
    if(!translation.isEmpty()) {
        if(!translation.filePaths().isEmpty()) {
            QString currentTranslationPath = qnSettings->translationPath();
            if(!translation.filePaths().contains(currentTranslationPath))
                qnSettings->setTranslationPath(translation.filePaths()[0]);
        }
    }
}

void QnLookAndFeelPreferencesWidget::updateFromSettings() {
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    m_oldSkin = ui->skinComboBox->findData(qnSettings->clientSkin());
    ui->skinComboBox->setCurrentIndex(m_oldSkin);

    ui->tourCycleTimeSpinBox->setValue(qnSettings->tourCycleTime() / 1000);
    ui->showIpInTreeCheckBox->setChecked(qnSettings->isIpShownInTree());

    ui->timeModeComboBox->setCurrentIndex(ui->timeModeComboBox->findData(qnSettings->timeMode()));

    m_oldLanguage = 0;
    QString translationPath = qnSettings->translationPath();
    for(int i = 0; i < ui->languageComboBox->count(); i++) {
        QnTranslation translation = ui->languageComboBox->itemData(i, Qn::TranslationRole).value<QnTranslation>();
        if(translation.filePaths().contains(translationPath)) {
            m_oldLanguage = i;
            break;
        }
    }
    ui->languageComboBox->setCurrentIndex(m_oldLanguage);

    bool backgroundAllowed = !(qnSettings->lightMode() & Qn::LightModeNoSceneBackground);
    ui->animationGroupBox->setEnabled(backgroundAllowed);
    ui->imageGroupBox->setEnabled(backgroundAllowed);

    QnClientBackground background = qnSettings->background();
    m_oldBackground = background;

    if (!backgroundAllowed) {
        ui->animationEnabledCheckBox->setChecked(false);
        ui->imageEnabledCheckBox->setChecked(false);
    } else {
        ui->animationEnabledCheckBox->setChecked(background.animationEnabled);
        ui->animationColorComboBox->setCurrentIndex(ui->animationColorComboBox->findData(qVariantFromValue(background.animationMode)));
        ui->colorSelectButton->setEnabled(background.animationMode == Qn::CustomAnimation);
        QColor customColor = background.animationCustomColor;
        if (!customColor.isValid())
            customColor = withAlpha(Qt::blue, 51);
        m_colorDialog->setCurrentColor(withAlpha(customColor, 255));
        ui->animationOpacitySpinBox->setValue(qRound(customColor.alphaF() * 100));
        updateAnimationCustomColor();

        ui->imageEnabledCheckBox->setChecked(background.imageEnabled);
        ui->imageNameLineEdit->setText(background.imageOriginalName);
        ui->imageModeComboBox->setCurrentIndex(ui->imageModeComboBox->findData(qVariantFromValue(background.imageMode)));
        ui->imageOpacitySpinBox->setValue(qRound(background.imageOpacity * 100));
    }
}

bool QnLookAndFeelPreferencesWidget::confirm() {
    return m_oldLanguage == ui->languageComboBox->currentIndex()
        && m_oldSkin == ui->skinComboBox->currentIndex();
}

bool QnLookAndFeelPreferencesWidget::discard() {
    bool backgroundAllowed = !(qnSettings->lightMode() & Qn::LightModeNoSceneBackground);
    if (backgroundAllowed)
        qnSettings->setBackground(m_oldBackground);
    return true;
}

QColor QnLookAndFeelPreferencesWidget::animationCustomColor() const {
    QColor color = m_colorDialog->currentColor();
    color.setAlphaF(0.01* ui->animationOpacitySpinBox->value());
    return color;
}

void QnLookAndFeelPreferencesWidget::updateAnimationCustomColor() {
    const int iconDim = 16;
    const QSize iconSize(iconDim, iconDim);
    const QRect iconRect(QPoint(0, 0), iconSize);
    {
        QPixmap pixmap(iconSize);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setPen(Qt::NoPen);
        painter.setBrush(withAlpha(animationCustomColor(), 255));
        painter.drawRoundedRect(iconRect, 50, 50, Qt::RelativeSize);
        ui->colorSelectButton->setIcon(pixmap);
    }

    if (!m_updating) {
        QnClientBackground background = qnSettings->background();
        background.animationCustomColor = animationCustomColor();
        qnSettings->setBackground(background);
    }
}

void QnLookAndFeelPreferencesWidget::selectBackgroundImage() {
    QString nameFilter;
    foreach (const QByteArray &format, QImageReader::supportedImageFormats()) {
        if (!nameFilter.isEmpty())
            nameFilter += QLatin1Char(' ');
        nameFilter += QLatin1String("*.") + QLatin1String(format);
    }
    nameFilter = QLatin1Char('(') + nameFilter + QLatin1Char(')');

    QScopedPointer<QnCustomFileDialog> dialog(
        new QnCustomFileDialog (
        this, tr("Select file..."),
        qnSettings->backgroundsFolder(),
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
    if (qnSettings->background().imageName == cachedName)
        return;

    QnProgressDialog* progressDialog = new QnProgressDialog(this);
    progressDialog->setWindowTitle(tr("Preparing Image..."));
    progressDialog->setLabelText(tr("Please wait while image is being prepared..."));
    progressDialog->setInfiniteProgress();
    progressDialog->setModal(true);

    QnLocalFileCache* imgCache = new QnLocalFileCache(this);
    connect(imgCache, &QnAppServerFileCache::fileUploaded, this, [this, imgCache, progressDialog, originalFileName](const QString &storedFileName) {
        if (!progressDialog->wasCanceled()) {
            QnClientBackground background = qnSettings->background();
            background.imageName = storedFileName;
            background.imageOriginalName = QFileInfo(originalFileName).fileName();
            qnSettings->setBackground(background);
            ui->imageNameLineEdit->setText( background.imageOriginalName);
        }
        imgCache->deleteLater();
        progressDialog->hide();
        progressDialog->deleteLater();
    });
 
    imgCache->storeImage(originalFileName);
    progressDialog->exec();
}

void QnLookAndFeelPreferencesWidget::setupLanguageUi() {
    QnTranslationListModel *model = new QnTranslationListModel(this);
    model->setTranslations(qnCommon->instance<QnClientTranslationManager>()->loadTranslations());
    ui->languageComboBox->setModel(model);

    setWarningStyle(ui->languageWarningLabel);
    ui->languageWarningLabel->setVisible(false);

    connect(ui->languageComboBox, QnComboboxCurrentIndexChanged,  this,   [this](int index) {
        ui->languageWarningLabel->setVisible(m_oldLanguage != index);
    });
}

void QnLookAndFeelPreferencesWidget::setupSkinUi() {
    ui->skinComboBox->addItem(tr("Dark"), Qn::DarkSkin);
    ui->skinComboBox->addItem(tr("Light"), Qn::LightSkin);
    setWarningStyle(ui->skinWarningLabel);
    ui->skinWarningLabel->setVisible(false);

    connect(ui->skinComboBox, QnComboboxCurrentIndexChanged,  this,   [this](int index) {
        ui->skinWarningLabel->setVisible(m_oldSkin != index);
    });
}

void QnLookAndFeelPreferencesWidget::setupTimeModeUi() {
    ui->timeModeComboBox->addItem(tr("Server Time"), Qn::ServerTimeMode);
    ui->timeModeComboBox->addItem(tr("Client Time"), Qn::ClientTimeMode);
    connect(ui->timeModeComboBox, QnComboboxActivated,  this,   [this](int index) {
        Qn::TimeMode selectedMode = static_cast<Qn::TimeMode>(ui->timeModeComboBox->itemData(index).toInt());
        ui->timeModeWarningLabel->setVisible(m_oldTimeMode == Qn::ServerTimeMode && selectedMode == Qn::ClientTimeMode);
    });
    setWarningStyle(ui->timeModeWarningLabel);
    ui->timeModeWarningLabel->setVisible(false);
}

void QnLookAndFeelPreferencesWidget::setupBackgroundUi() {
    ui->animationColorComboBox->addItem(tr("Default"),  qVariantFromValue(Qn::DefaultAnimation));
    ui->animationColorComboBox->addItem(tr("Rainbow"),  qVariantFromValue(Qn::RainbowAnimation));
    ui->animationColorComboBox->addItem(tr("Custom..."),qVariantFromValue(Qn::CustomAnimation));

    ui->imageModeComboBox->addItem(tr("Stretch"), qVariantFromValue(Qn::StretchImage));
    ui->imageModeComboBox->addItem(tr("Fit"),     qVariantFromValue(Qn::FitImage));
    ui->imageModeComboBox->addItem(tr("Crop"),    qVariantFromValue(Qn::CropImage));

    connect(ui->animationEnabledCheckBox, &QCheckBox::toggled, this, [this] (bool checked) {
        ui->animationWidget->setEnabled(checked);

        if (m_updating)
            return;

        action(Qn::ToggleBackgroundAnimationAction)->setChecked(checked);

        QnClientBackground background = qnSettings->background();
        background.animationEnabled = checked;
        qnSettings->setBackground(background);
    });

    connect(ui->imageEnabledCheckBox, &QCheckBox::toggled, this, [this] (bool checked) {
        ui->imageWidget->setEnabled(checked);

        if (m_updating)
            return;

        QnClientBackground background = qnSettings->background();
        background.imageEnabled = checked;
        qnSettings->setBackground(background);
    });

    connect(ui->animationColorComboBox, QnComboboxCurrentIndexChanged,  this,   [this] {
        if (m_updating)
            return;
        Qn::BackgroundAnimationMode selected = ui->animationColorComboBox->currentData().value<Qn::BackgroundAnimationMode>();
        ui->colorSelectButton->setEnabled(selected == Qn::CustomAnimation);
        QnClientBackground background = qnSettings->background();
        background.animationMode = selected;
        qnSettings->setBackground(background);
    });

    connect(ui->colorSelectButton,  &QPushButton::clicked, this, [this] {
        if (!m_colorDialog->exec())
            return;
        updateAnimationCustomColor();
    });

    connect(ui->animationOpacitySpinBox,  QnSpinboxIntValueChanged, this,   &QnLookAndFeelPreferencesWidget::updateAnimationCustomColor);
    connect(ui->imageSelectButton,        &QPushButton::clicked,    this,   &QnLookAndFeelPreferencesWidget::selectBackgroundImage);
    connect(ui->imageModeComboBox,        QnComboboxCurrentIndexChanged,  this,   [this] {
        if (m_updating)
            return;

        QnClientBackground background = qnSettings->background();
        background.imageMode = ui->imageModeComboBox->currentData().value<Qn::ImageBehaviour>();
        qnSettings->setBackground(background);
    });

    connect(ui->imageOpacitySpinBox,      QnSpinboxIntValueChanged, this,   [this](int value) {
        if (m_updating)
            return;

        QnClientBackground background = qnSettings->background();
        background.imageOpacity = 0.01 * value;
        qnSettings->setBackground(background);
    });
}
