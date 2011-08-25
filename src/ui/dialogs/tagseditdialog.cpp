#include "tagseditdialog.h"
#include "ui_tagseditdialog.h"

#include <QtGui/QStringListModel>
#include <QtGui/QSortFilterProxyModel>

#include "base/tagmanager.h"

TagsEditDialog::TagsEditDialog(const QString &objectId, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TagsEditDialog),
    m_objectId(objectId)
{
    ui->setupUi(this);

    m_objectTagsModel = new QStringListModel;
    m_allTagsModel = new QStringListModel;

    m_allTagsProxyModel = new QSortFilterProxyModel;
    m_allTagsProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_allTagsProxyModel->setSourceModel(m_allTagsModel);

    ui->objectTagsListView->setModel(m_objectTagsModel);
    ui->objectTagsListView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    ui->allTagsListView->setModel(m_allTagsProxyModel);
    ui->allTagsListView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    ui->clearFilterButton->setVisible(!ui->tagsFilterLineEdit->text().isEmpty());

    connect(ui->tagsFilterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(filterChanged(QString)));
    connect(ui->clearFilterButton, SIGNAL(clicked()), ui->tagsFilterLineEdit, SLOT(clear()));

    connect(ui->newTagLineEdit, SIGNAL(returnPressed()), ui->addTagButton, SLOT(animateClick()));
    connect(ui->addTagButton, SIGNAL(clicked()), this, SLOT(addTag()));
    connect(ui->addTagsButton, SIGNAL(clicked()), this, SLOT(addTags()));
    connect(ui->removeTagsButton, SIGNAL(clicked()), this, SLOT(removeTags()));

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    if (QPushButton *button = ui->buttonBox->button(QDialogButtonBox::Reset))
        connect(button, SIGNAL(clicked()), this, SLOT(reset()));

    reset();

    ui->newTagLineEdit->setFocus();
}

TagsEditDialog::~TagsEditDialog()
{
    delete ui;
}

void TagsEditDialog::accept()
{
    foreach (const QString &tag, TagManager::objectTags(m_objectId))
        TagManager::removeObjectTag(m_objectId, tag);

    foreach (const QString &tag, m_objectTagsModel->stringList())
        TagManager::addObjectTag(m_objectId, tag);

    QDialog::accept();
}

void TagsEditDialog::reset()
{
    QStringList objectTags = TagManager::objectTags(m_objectId);
    m_objectTagsModel->setStringList(objectTags);

    QStringList allTags = TagManager::allTags();
    foreach (const QString &tag, objectTags)
        allTags.removeOne(tag);
    m_allTagsModel->setStringList(allTags);
}

void TagsEditDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);

    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void TagsEditDialog::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return)
        return;

    QDialog::keyPressEvent(e);
}

void TagsEditDialog::addTag()
{
    QStringList tags;
    tags << ui->newTagLineEdit->text().trimmed().toLower();
    addTags(tags);

    ui->newTagLineEdit->clear();
}

void TagsEditDialog::addTags()
{
    QStringList tags;
    foreach (const QModelIndex &index, ui->allTagsListView->selectionModel()->selectedRows())
        tags << index.data().toString();
    addTags(tags);
}

void TagsEditDialog::removeTags()
{
    QStringList tags;
    foreach (const QModelIndex &index, ui->objectTagsListView->selectionModel()->selectedRows())
        tags << index.data().toString();
    removeTags(tags);
}

void TagsEditDialog::filterChanged(const QString &filter)
{
    if (!filter.isEmpty())
    {
        ui->clearFilterButton->show();
        m_allTagsProxyModel->setFilterWildcard(QLatin1Char('*') + filter + QLatin1Char('*'));
    }
    else
    {
        ui->clearFilterButton->hide();
        m_allTagsProxyModel->setFilterFixedString(filter);
    }
}

void TagsEditDialog::addTags(const QStringList &tags)
{
    QStringList objectTags = m_objectTagsModel->stringList();
    QStringList allTags = m_allTagsModel->stringList();

    foreach (const QString &tag, tags)
    {
        if (tag.isEmpty() || objectTags.contains(tag))
            continue;

        objectTags.append(tag);
        allTags.removeOne(tag);
    }

    qSort(objectTags);

    m_objectTagsModel->setStringList(objectTags);
    m_allTagsModel->setStringList(allTags);
}

void TagsEditDialog::removeTags(const QStringList &tags)
{
    QStringList objectTags = m_objectTagsModel->stringList();
    QStringList allTags = m_allTagsModel->stringList();

    foreach (const QString &tag, tags)
    {
        if (tag.isEmpty() || !objectTags.contains(tag))
            continue;

        objectTags.removeOne(tag);
        allTags.append(tag);
    }

    qSort(allTags);

    m_objectTagsModel->setStringList(objectTags);
    m_allTagsModel->setStringList(allTags);
}
