#include "custom_file_dialog.h"

#include <QtGui/QImageReader>

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>

#include <nx/utils/string.h>

namespace {

static QString kSpinBoxPlaceholder("%value%");

class FileExtensions
{
    Q_DECLARE_TR_FUNCTIONS(FileExtensions)

public:
    static QnCustomFileDialog::FileFilter createPicturesFilter()
    {
        QStringList extensions;
        for (const QByteArray& format: QImageReader::supportedImageFormats())
            extensions.push_back(QString::fromLatin1(format));
        return {tr("Pictures"), extensions};
    }

    static QnCustomFileDialog::FileFilter createVideoFilter()
    {
        static const QStringList kVideoFormats{"avi", "mkv", "mp4", "mov", "ts", "m2ts", "mpeg", "mpg",
            "flv", "wmv", "3gp"};
        return {tr("Video"), kVideoFormats};
    }

    static QnCustomFileDialog::FileFilter createAllFilesFilter()
    {
        return {tr("All Files"), {"*"}};
    }
};

} // namespace

const QnCustomFileDialog::FileFilter QnCustomFileDialog::kPicturesFilter(
    FileExtensions::createPicturesFilter());
const QnCustomFileDialog::FileFilter QnCustomFileDialog::kVideoFilter(
    FileExtensions::createVideoFilter());
const QnCustomFileDialog::FileFilter QnCustomFileDialog::kAllFilesFilter(
    FileExtensions::createAllFilesFilter());

// TODO: #GDM include and use <ui/workaround/cancel_drag.h>
QnCustomFileDialog::QnCustomFileDialog(
    QWidget* parent,
    const QString& caption,
    const QString& directory,
    const QString& filter):
    base_type(parent, caption, directory, filter)
{
    setOptions(fileDialogOptions());
    connect(this, &QFileDialog::accepted, this, &QnCustomFileDialog::at_accepted);
}

QnCustomFileDialog::~QnCustomFileDialog()
{
}

QString QnCustomFileDialog::createFilter(std::vector<FileFilter> filters)
{
    QStringList filterEntries;
    for (const auto& filter: filters)
    {
        QStringList extensions;
        std::transform(filter.second.cbegin(), filter.second.cend(),
            std::back_inserter(extensions),
            [](const QString& extension) { return "*." + extension; });

        filterEntries.push_back(filter.first + " (" + extensions.join(' ') + ")");
    }
    return filterEntries.join(";;");
}

QString QnCustomFileDialog::createFilter(FileFilter filter)
{
    return createFilter(std::vector<FileFilter>{filter});
}

QString QnCustomFileDialog::createFilter(QString title, QStringList extensions)
{
    return createFilter({{title, extensions}});
}

QString QnCustomFileDialog::createFilter(QString title, const QString& extension)
{
    return createFilter(title, QStringList(extension));
}

void QnCustomFileDialog::addCheckBox(
    const QString& text,
    bool* value,
    QnAbstractWidgetControlDelegate* delegate)
{
    auto checkbox = new QCheckBox(this);
    checkbox->setText(text);
    checkbox->setChecked(*value);
    m_checkBoxes.emplace(checkbox, value);
    addWidget(QString(), checkbox, delegate);
}

void QnCustomFileDialog::addSpinBox(const QString& text, int minValue, int maxValue, int* value)
{
    auto widget = new QWidget(this);
    auto layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    const int index = text.indexOf(kSpinBoxPlaceholder);
    const QString prefix = text.mid(0, index).trimmed();
    const QString postfix = index >= 0
        ? text.mid(index + kSpinBoxPlaceholder.length()).trimmed()
        : QString();

    auto labelPrefix = new QLabel(widget);
    labelPrefix->setText(prefix);
    layout->addWidget(labelPrefix);

    auto spinbox = new QSpinBox(widget);
    spinbox->setMinimum(minValue);
    spinbox->setMaximum(maxValue);
    spinbox->setValue(*value);
    m_spinBoxes.emplace(spinbox, value);
    layout->addWidget(spinbox);

    if (!postfix.isEmpty())
    {
        auto labelPostfix = new QLabel(widget);
        labelPostfix->setText(postfix);
        layout->addWidget(labelPostfix);
    }

    layout->addStretch();

    widget->setLayout(layout);

    addWidget(QString(), widget);
}

QString QnCustomFileDialog::spinBoxPlaceholder()
{
    return kSpinBoxPlaceholder;
}

void QnCustomFileDialog::addLineEdit(const QString& label, QString* value)
{
    auto lineEdit = new QLineEdit(this);
    lineEdit->setText(*value);
    m_lineEdits.emplace(lineEdit, value);

    addWidget(label, lineEdit);
}

void QnCustomFileDialog::addWidget(
    const QString& label,
    QWidget* widget,
    QnAbstractWidgetControlDelegate* delegate)
{
    QGridLayout* layout = customizedLayout();
    NX_ASSERT(layout);

    const int row = layout->rowCount();
    if (label.isEmpty())
    {
        layout->addWidget(widget, row, 0, 1, 2);
    }
    else
    {
        layout->addWidget(new QLabel(label, this), row, 0);
        layout->addWidget(widget, row, 1);
    }
    widget->raise();

    if (delegate)
    {
        delegate->addWidget(widget);
        delegate->disconnect();
        connect(
            this,
            &QnSystemBasedCustomDialog::filterSelected,
            delegate,
            &QnAbstractWidgetControlDelegate::updateWidget);
    }
}

void QnCustomFileDialog::at_accepted()
{
    for (auto [checkbox, value]: m_checkBoxes)
        *value = checkbox->isChecked();

    for (auto [spinbox, value]: m_spinBoxes)
        *value = spinbox->value();

    for (auto [lineedit, value]: m_lineEdits)
        *value = lineedit->text();
}

QString QnCustomFileDialog::selectedExtension() const
{
    return nx::utils::extractFileExtension(selectedNameFilter());
}
