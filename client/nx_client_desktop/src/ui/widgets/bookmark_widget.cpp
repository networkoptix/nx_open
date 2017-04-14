#include "bookmark_widget.h"
#include "ui_bookmark_widget.h"

#include <QtCore/QDateTime>

#include <core/resource/camera_bookmark.h>

#include <ui/style/custom_style.h>
#include <ui/utils/validators.h>

namespace {
    const int defaultTimeoutIdx = 0;
}

QnBookmarkWidget::QnBookmarkWidget(QWidget *parent):
    QWidget(parent),
    ui(new Ui::BookmarkWidget),
    m_isValid(false)
{
    ui->setupUi(this);

    connect(ui->tagsListLabel, &QLabel::linkActivated, this, [this](const QString &link) {
        if (m_selectedTags.contains(link))
            m_selectedTags.remove(link);
        else
            m_selectedTags.insert(link.trimmed());
        ui->tagsLineEdit->setText(QnCameraBookmark::tagsToString(m_selectedTags));
        updateTagsList();
    });

    connect(ui->tagsLineEdit, &QLineEdit::textEdited, this, [this](const QString &text)
    {
        m_selectedTags.clear();
        for(auto &tag: text.split(L',', QString::SkipEmptyParts).toSet())
            m_selectedTags.insert(tag.trimmed());

        updateTagsList();
    });

    ui->nameInputField->setValidator(Qn::defaultNonEmptyValidator(tr("Name cannot be empty.")));

    connect(ui->nameInputField, &QnInputField::textChanged, this, [this]()
    {
        bool wasValid = m_isValid;
        m_isValid = ui->nameInputField->isValid();

        if (wasValid != m_isValid)
            emit validChanged();
    });

    // TODO: #3.0 #rvasilenko Remove when bookmark timeout will be implemented.
    // Then change defaultTimeoutIdx constant value to '3'.
    ui->timeoutComboBox->hide();
    ui->timeoutLabel->hide();
}

QnBookmarkWidget::~QnBookmarkWidget() {}

const QnCameraBookmarkTagList &QnBookmarkWidget::tags() const {
    return m_allTags;
}

void QnBookmarkWidget::setTags(const QnCameraBookmarkTagList &tags) {
    m_allTags = tags;
    updateTagsList();
}

void QnBookmarkWidget::loadData(const QnCameraBookmark &bookmark) {
    ui->nameInputField->setText(bookmark.name);
    ui->descriptionTextEdit->setPlainText(bookmark.description);

    QDateTime start = QDateTime::fromMSecsSinceEpoch(bookmark.startTimeMs);
    ui->timeoutComboBox->clear();
    ui->timeoutComboBox->addItem(tr("Do not lock archive"), 0);
    ui->timeoutComboBox->addItem(tr("1 month"), start.addMonths(1).toMSecsSinceEpoch() - bookmark.startTimeMs);
    ui->timeoutComboBox->addItem(tr("3 month"), start.addMonths(3).toMSecsSinceEpoch() - bookmark.startTimeMs);
    ui->timeoutComboBox->addItem(tr("6 month"), start.addMonths(6).toMSecsSinceEpoch() - bookmark.startTimeMs);
    ui->timeoutComboBox->addItem(tr("year"), start.addYears(1).toMSecsSinceEpoch() - bookmark.startTimeMs);

    int timeoutIdx = ui->timeoutComboBox->findData(bookmark.timeout);
    ui->timeoutComboBox->setCurrentIndex(timeoutIdx < 0 ? defaultTimeoutIdx : timeoutIdx);

    m_selectedTags = bookmark.tags;
    ui->tagsLineEdit->setText(QnCameraBookmark::tagsToString(bookmark.tags));

    updateTagsList();
}

void QnBookmarkWidget::submitData(QnCameraBookmark &bookmark) const {
    bookmark.name = ui->nameInputField->text().trimmed();
    bookmark.description = ui->descriptionTextEdit->toPlainText().trimmed();
    bookmark.timeout = ui->timeoutComboBox->currentData().toLongLong();
    bookmark.tags = m_selectedTags;
}

bool QnBookmarkWidget::isValid() const
{
    return ui->nameInputField->isValid();
}

void QnBookmarkWidget::updateTagsList() {
    QString html = lit("<html><body><center>%1</center></body></html>");
    QString unusedTag = lit("<a style=\"text-decoration:none;\" href=\"%1\">%1</a><span style=\"text-decoration:none;\"> </span>");
    QString usedTag = lit("<a style=\"text-decoration:none;\" href=\"%1\"><font style=\"color:#009933\">%1</font></a><span style=\"text-decoration:none;\"> </span>");

    QStringList tags;
    for (const auto& tag : m_allTags)
    {
        if (m_selectedTags.contains(tag.name))
            tags << usedTag.arg(tag.name.toHtmlEscaped());
        else
            tags << unusedTag.arg(tag.name.toHtmlEscaped());
    }

    ui->tagsListLabel->setText(html.arg(tags.join(lit(", "))));
    update();
}
