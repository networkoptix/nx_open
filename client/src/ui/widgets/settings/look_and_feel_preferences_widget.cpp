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

namespace {

    class QnColorComboBoxItemDelegate: public QStyledItemDelegate {
        typedef QStyledItemDelegate base_type;
    public:
        explicit QnColorComboBoxItemDelegate(QObject *parent = 0): base_type(parent) {}
        ~QnColorComboBoxItemDelegate() {}

        virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) {
            base_type::paint(painter, option, index);
        }

        virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
            QSize result = base_type::sizeHint(option, index);
            result += QSize(48, 0); 
            return result;
        }

    };
}

QnLookAndFeelPreferencesWidget::QnLookAndFeelPreferencesWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::LookAndFeelPreferencesWidget),
    m_colorDialog(new QColorDialog()),
    m_updating(false),
    m_oldLanguage(0),
    m_oldSkin(0),
    m_oldBackgroundMode(Qn::DefaultBackground),
    m_oldBackgroundImageOpacity(0.5),
    m_oldBackgroundImageMode(Qn::StretchImage)
{
    ui->setupUi(this);

    ui->timeModeComboBox->addItem(tr("Server Time"), Qn::ServerTimeMode);
    ui->timeModeComboBox->addItem(tr("Client Time"), Qn::ClientTimeMode);

    ui->skinComboBox->addItem(tr("Dark"), Qn::DarkSkin);
    ui->skinComboBox->addItem(tr("Light"), Qn::LightSkin);

    setHelpTopic(ui->tourCycleTimeLabel,      ui->tourCycleTimeSpinBox,       Qn::SystemSettings_General_TourCycleTime_Help);
    setHelpTopic(ui->showIpInTreeLabel,       ui->showIpInTreeCheckBox,       Qn::SystemSettings_General_ShowIpInTree_Help);
    setHelpTopic(ui->languageLabel,           ui->languageComboBox,           Qn::SystemSettings_General_Language_Help);
    setHelpTopic(ui->lookAndFeelGroupBox,                                     Qn::SystemSettings_General_Customizing_Help);

    initTranslations();

    setWarningStyle(ui->languageWarningLabel);
    setWarningStyle(ui->skinWarningLabel);
    ui->languageWarningLabel->setVisible(false);
    ui->skinWarningLabel->setVisible(false);

    connect(ui->timeModeComboBox,                       QnComboboxActivated,            this,   &QnLookAndFeelPreferencesWidget::at_timeModeComboBox_activated);
    connect(ui->languageComboBox,                       QnComboboxCurrentIndexChanged,  this,   [this](int index) {
        ui->languageWarningLabel->setVisible(m_oldLanguage != index);
    });
    connect(ui->skinComboBox,                           QnComboboxCurrentIndexChanged,  this,   [this](int index) {
        ui->skinWarningLabel->setVisible(m_oldSkin != index);
    });

  /*  QButtonGroup* buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(ui->backgroundEmptyRadioButton,      Qn::NoBackground);
    buttonGroup->addButton(ui->backgroundDefaultRadioButton,    Qn::DefaultBackground);
    buttonGroup->addButton(ui->backgroundRainbowRadioButton,    Qn::RainbowBackground);
    buttonGroup->addButton(ui->backgroundCustomRadioButton,     Qn::CustomColorBackground);
    buttonGroup->addButton(ui->backgroundImageRadioButton,      Qn::ImageBackground);

    connect(buttonGroup,    QnButtonGroupIdToggled, this, [this](int id, bool toggled) {
        if (!toggled)
            return;

        action(Qn::ToggleBackgroundAnimationAction)->setChecked(id != Qn::NoBackground && id != Qn::ImageBackground);
        qnSettings->setBackgroundMode(static_cast<Qn::ClientBackground>(id));
    });

    connect(ui->selectColorButton,                      &QPushButton::clicked,          this,   [this] {
        if (m_colorDialog->exec())
            updateBackgroundColor();
    });

    connect(ui->backgroundColorOpacitySpinBox,          QnSpinboxIntValueChanged,       this,   &QnLookAndFeelPreferencesWidget::updateBackgroundColor);

    connect(ui->selectImageButton,                      &QPushButton::clicked,          this,   &QnLookAndFeelPreferencesWidget::selectBackgroundImage);
    connect(ui->backgroundImageOpacitySpinBox,          QnSpinboxIntValueChanged,       this,   [this](int value) {
        if (!m_updating)
            qnSettings->setBackgroundImageOpacity(0.01 * value);
    });
    */
    ui->backgroundImageModeComboBox->addItem(tr("Stretch"), qVariantFromValue(Qn::StretchImage));
    ui->backgroundImageModeComboBox->addItem(tr("Fit"),     qVariantFromValue(Qn::FitImage));
    ui->backgroundImageModeComboBox->addItem(tr("Crop"),    qVariantFromValue(Qn::CropImage));

    
    int iconDim = 16;
    QSize iconSize(iconDim, iconDim);
    QRect iconRect(QPoint(0, 0), iconSize);
    {
        QPixmap pixmap(iconSize);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::blue);
        painter.drawRoundedRect(iconRect, 50, 50, Qt::RelativeSize);

        ui->colorComboBox->addItem(pixmap, tr("Default"), qVariantFromValue(Qn::DefaultBackground));
    }

    {
        QVector<QColor> colors;
        colors << 
            QColor(0xFFFF0000) <<
            QColor(0xFFFF7F00) <<
            QColor(0xFFFFFF00) <<
            QColor(0xFF00FF00) <<
            QColor(0xFF0000FF) <<
            QColor(0xFF4B0082) <<
            QColor(0xFF8B00FF);

        QLinearGradient rainbow(0, 0, iconDim, iconDim);
        qreal pos = 0.0;
        qreal step = 1.0 / (colors.size() - 1);
        for(const QColor &color: colors) {
            rainbow.setColorAt(pos, color);
            pos += step;
        }

        QPixmap pixmap(iconSize);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setPen(Qt::NoPen);
        painter.setBrush(rainbow);
        painter.drawRoundedRect(iconRect, 50, 50, Qt::RelativeSize);

        ui->colorComboBox->addItem(pixmap, tr("Rainbow"),   qVariantFromValue(Qn::RainbowBackground));
    }

    {
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::red);
        ui->colorComboBox->addItem(pixmap, tr("Custom..."), qVariantFromValue(Qn::CustomColorBackground));
    } 

    /*
    connect(ui->backgroundImageModeComboBox,            QnComboboxCurrentIndexChanged,  this,   [this] {
        if (m_updating)
            return;
        qnSettings->setBackgroundImageMode(ui->backgroundImageModeComboBox->currentData().value<Qn::ImageBehaviour>());
    });*/
}

QnLookAndFeelPreferencesWidget::~QnLookAndFeelPreferencesWidget()
{
}

bool QnLookAndFeelPreferencesWidget::event(QEvent *event) {
    bool result = base_type::event(event);

    if(event->type() == QEvent::Show)
        alignGrids();

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
    m_oldBackgroundMode = qnSettings->backgroundMode();
    m_oldCustomBackgroundColor = qnSettings->customBackgroundColor();
    m_oldBackgroundImage = qnSettings->backgroundImage();
    m_oldBackgroundImageOpacity = qnSettings->backgroundImageOpacity();
    m_oldBackgroundImageMode = qnSettings->backgroundImageMode();

 /*   if (!backgroundAllowed) {
        ui->backgroundEmptyRadioButton->setChecked(true);
    } else {
        switch (qnSettings->backgroundMode()) {
        case Qn::NoBackground:
            ui->backgroundEmptyRadioButton->setChecked(true);
            break;
        case Qn::RainbowBackground:
            ui->backgroundRainbowRadioButton->setChecked(true);
            break;
        case Qn::CustomColorBackground:
            ui->backgroundCustomRadioButton->setChecked(true);
            break;
        case Qn::ImageBackground:
            ui->backgroundImageRadioButton->setChecked(true);
            break;
        default:
            ui->backgroundDefaultRadioButton->setChecked(true);
            break;
        }
    }

    QColor customColor = qnSettings->customBackgroundColor();
    if (!customColor.isValid())
        customColor = withAlpha(Qt::darkBlue, 64);

    m_colorDialog->setCurrentColor(withAlpha(customColor, 255));
    ui->backgroundColorOpacitySpinBox->setValue(qRound(customColor.alphaF() * 100));
    ui->backgroundImageOpacitySpinBox->setValue(qRound(qnSettings->backgroundImageOpacity() * 100));

    ui->backgroundImageModeComboBox->setCurrentIndex(ui->backgroundImageModeComboBox->findData(qVariantFromValue(qnSettings->backgroundImageMode())));

    updateBackgroundColor();*/
}

bool QnLookAndFeelPreferencesWidget::confirm() {
    return m_oldLanguage == ui->languageComboBox->currentIndex()
        && m_oldSkin == ui->skinComboBox->currentIndex();
}

bool QnLookAndFeelPreferencesWidget::discard() {
    bool backgroundAllowed = !(qnSettings->lightMode() & Qn::LightModeNoSceneBackground);
    if (backgroundAllowed) {
        qnSettings->setBackgroundMode(m_oldBackgroundMode);
        qnSettings->setCustomBackgroundColor(m_oldCustomBackgroundColor);
        qnSettings->setBackgroundImage(m_oldBackgroundImage);
        qnSettings->setBackgroundImageOpacity(m_oldBackgroundImageOpacity);
        qnSettings->setBackgroundImageMode(m_oldBackgroundImageMode);
    }
    return true;
}


void QnLookAndFeelPreferencesWidget::initTranslations() {
    QnTranslationListModel *model = new QnTranslationListModel(this);
    model->setTranslations(qnCommon->instance<QnClientTranslationManager>()->loadTranslations());
    ui->languageComboBox->setModel(model);
}

QColor QnLookAndFeelPreferencesWidget::backgroundColor() const {
    QColor color = m_colorDialog->currentColor();
    color.setAlphaF(0.01* ui->backgroundColorOpacitySpinBox->value());
    return color;
}

void QnLookAndFeelPreferencesWidget::updateBackgroundColor() {
  /*  QPixmap pixmap(16, 16);
    pixmap.fill(withAlpha(backgroundColor(), 255));
    ui->selectColorButton->setIcon(pixmap);

    if (!m_updating)
        qnSettings->setCustomBackgroundColor(backgroundColor());*/
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

    QString fileName = dialog->selectedFile();
    if (fileName.isEmpty())
        return;

    qnSettings->setBackgroundsFolder(QFileInfo(fileName).absolutePath());


    QString cachedName = QnAppServerImageCache::cachedImageFilename(fileName);
    if (qnSettings->backgroundImage() == cachedName)
        return;

    QnProgressDialog* progressDialog = new QnProgressDialog(this);
    progressDialog->setWindowTitle(tr("Preparing Image..."));
    progressDialog->setLabelText(tr("Please wait while image is being prepared..."));
    progressDialog->setInfiniteProgress();
    progressDialog->setModal(true);

    QnLocalFileCache* imgCache = new QnLocalFileCache(this);
    connect(imgCache, &QnAppServerFileCache::fileUploaded, this, [this, imgCache, progressDialog](const QString &filename) {
        if (!progressDialog->wasCanceled())
            qnSettings->setBackgroundImage(filename);
        imgCache->deleteLater();
        progressDialog->hide();
        progressDialog->deleteLater();
    });
 
    imgCache->storeImage(fileName);
    progressDialog->exec();
}

void QnLookAndFeelPreferencesWidget::alignGrids() {
    QList<QGroupBox*> groupBoxes = this->findChildren<QGroupBox *>();

    int maxLabelWidth = 0;
    int maxRowHeight = 0;

    /* Calculate max sizes. */
    for (QGroupBox* groupBox: groupBoxes) {
        QGridLayout* layout = qobject_cast<QGridLayout*>(groupBox->layout());
        if (!layout)
            continue;

        for (int row = 0; row < layout->rowCount(); ++row) {            
            QRect labelCellRect = layout->cellRect(row, 0);
            if (labelCellRect.isValid()) {
                maxLabelWidth = qMax(maxLabelWidth, labelCellRect.width());
                maxRowHeight = qMax(maxRowHeight, labelCellRect.height());
            }
        }
    }

    /* Apply calculated sizes. */
    for (QGroupBox* groupBox: groupBoxes) {
        QGridLayout* layout = qobject_cast<QGridLayout*>(groupBox->layout());
        if (!layout)
            continue;

        if (maxLabelWidth > 0)
            layout->setColumnMinimumWidth(0, maxLabelWidth);

        if (maxRowHeight > 0)
            for (int row = 0; row < layout->rowCount(); ++row) 
                layout->setRowMinimumHeight(row, maxRowHeight);
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnLookAndFeelPreferencesWidget::at_timeModeComboBox_activated() {
    if(ui->timeModeComboBox->itemData(ui->timeModeComboBox->currentIndex(), Qt::UserRole).toInt() == Qn::ClientTimeMode) {
        QMessageBox::warning(this, tr("Warning"), tr("This option will not affect Recording Schedule. \nRecording Schedule is always based on Server Time."));
    }
}
