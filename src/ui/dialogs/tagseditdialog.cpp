#include "tagseditdialog.h"
#include "ui_tagseditdialog.h"

#include <QtCore/QSet>

#include <QtGui/QSortFilterProxyModel>
#include <QtGui/QStandardItemModel>

#include "base/tagmanager.h"
#include "ui/skin.h"

TagsEditDialog::TagsEditDialog(const QStringList &objectIds, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TagsEditDialog),
    m_objectIds(objectIds)
{
    setWindowTitle(tr("Edit File Tags"));

    ui->setupUi(this);

    m_objectTagsModel = new QStandardItemModel;
    m_allTagsModel = new QStandardItemModel;

    m_allTagsProxyModel = new QSortFilterProxyModel;
    m_allTagsProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_allTagsProxyModel->setSourceModel(m_allTagsModel);

    ui->objectTagsTreeView->setModel(m_objectTagsModel);
    ui->objectTagsTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->objectTagsTreeView->setRootIsDecorated(false);
    ui->objectTagsTreeView->setHeaderHidden(false);
    ui->objectTagsTreeView->setSortingEnabled(false);
    ui->objectTagsTreeView->setUniformRowHeights(true);
    ui->objectTagsTreeView->setWordWrap(false);

    ui->allTagsTreeView->setModel(m_allTagsProxyModel);
    ui->allTagsTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->allTagsTreeView->setRootIsDecorated(false);
    ui->allTagsTreeView->setHeaderHidden(false);
    ui->allTagsTreeView->setSortingEnabled(false);
    ui->allTagsTreeView->setUniformRowHeights(true);
    ui->allTagsTreeView->setWordWrap(false);

    ui->clearFilterButton->setVisible(!ui->tagsFilterLineEdit->text().isEmpty());

    ui->addTagsButton->setIcon(Skin::icon(QLatin1String("left-arrow.png")));
    ui->removeTagsButton->setIcon(Skin::icon(QLatin1String("skin/right-arrow.png")));
    ui->addTagButton->setIcon(Skin::icon(QLatin1String("skin/plus.png")));
    ui->clearFilterButton->setIcon(Skin::icon(QLatin1String("skin/close2.png")));

    connect(ui->tagsFilterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(filterChanged(QString)));
    connect(ui->clearFilterButton, SIGNAL(clicked()), ui->tagsFilterLineEdit, SLOT(clear()));

    connect(ui->newTagLineEdit, SIGNAL(returnPressed()), ui->addTagButton, SLOT(animateClick()));
    connect(ui->addTagButton, SIGNAL(clicked()), this, SLOT(addTag()));
    connect(ui->addTagsButton, SIGNAL(clicked()), this, SLOT(addTags()));
    connect(ui->removeTagsButton, SIGNAL(clicked()), this, SLOT(removeTags()));

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    if (QPushButton *button = ui->buttonBox->button(QDialogButtonBox::RestoreDefaults))
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
    QSet<QString> oldIntersectedObjectTagsSet;
    if (!m_objectIds.isEmpty())
    {
        oldIntersectedObjectTagsSet = TagManager::objectTags(m_objectIds.at(0)).toSet();
        for (int i = 1; i < m_objectIds.size(); ++i)
            oldIntersectedObjectTagsSet = oldIntersectedObjectTagsSet.intersect(TagManager::objectTags(m_objectIds.at(i)).toSet());
    }

    QSet<QString> intersectedObjectTagsSet;
    for (int row = 0; row < m_objectTagsModel->rowCount(); ++row)
    {
        QStandardItem *item = m_objectTagsModel->item(row);
        if (item->flags() != Qt::NoItemFlags)
            intersectedObjectTagsSet.insert(item->text());
    }

    foreach (const QString &objectId, m_objectIds)
    {
        QSet<QString> oldObjectTags = TagManager::objectTags(objectId).toSet();
        QSet<QString> objectTags = oldObjectTags;
        if (!oldIntersectedObjectTagsSet.isEmpty())
            objectTags = objectTags.subtract(oldIntersectedObjectTagsSet.subtract(intersectedObjectTagsSet));
        if (!intersectedObjectTagsSet.isEmpty())
            objectTags = objectTags.unite(intersectedObjectTagsSet.subtract(oldIntersectedObjectTagsSet));
        if (objectTags != oldObjectTags)
            TagManager::setObjectTags(objectId, objectTags.toList());
    }

    QDialog::accept();
}

void TagsEditDialog::reset()
{
    m_objectTagsModel->clear();
    m_allTagsModel->clear();

    retranslateUi();

    QSet<QString> intersectedObjectTagsSet;
    if (!m_objectIds.isEmpty())
    {
        QSet<QString> allObjectTagsSet = intersectedObjectTagsSet = TagManager::objectTags(m_objectIds.at(0)).toSet();
        for (int i = 1; i < m_objectIds.size(); ++i)
        {
            QSet<QString> objectTags = TagManager::objectTags(m_objectIds.at(i)).toSet();
            intersectedObjectTagsSet = intersectedObjectTagsSet.intersect(objectTags);
            allObjectTagsSet = allObjectTagsSet.unite(objectTags);
        }

        QStringList allObjectTags = allObjectTagsSet.toList();
        qSort(allObjectTags);
        foreach (const QString &tag, allObjectTags)
        {
            QStandardItem *item = new QStandardItem(tag);
            if (intersectedObjectTagsSet.contains(tag))
                item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            else
                item->setFlags(Qt::NoItemFlags);
            m_objectTagsModel->appendRow(item);
        }
    }

    QStringList allTags = TagManager::allTags();
    qSort(allTags);
    foreach (const QString &tag, allTags)
    {
        QStandardItem *item = new QStandardItem(tag);
        if (!intersectedObjectTagsSet.contains(tag))
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        else
            item->setFlags(Qt::NoItemFlags);
        m_allTagsModel->appendRow(item);
    }
}

void TagsEditDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);

    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        retranslateUi();
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

void TagsEditDialog::showEvent(QShowEvent *e)
{
    QDialog::showEvent(e);

    if (ui->newTagLineEdit->height() != ui->tagsFilterLineEdit->height())
    {
        const int maxHeight = qMax(ui->newTagLineEdit->height(), ui->tagsFilterLineEdit->height());
        ui->newTagLineEdit->setMaximumHeight(maxHeight);
        ui->tagsFilterLineEdit->setMaximumHeight(maxHeight);
        if (ui->addTagButton->height() > maxHeight)
            ui->addTagButton->setMaximumSize(maxHeight, maxHeight);
        if (ui->clearFilterButton->height() > maxHeight)
            ui->clearFilterButton->setMaximumSize(maxHeight, maxHeight);
    }
}

void TagsEditDialog::retranslateUi()
{
    m_objectTagsModel->setHorizontalHeaderLabels(QStringList() << tr("Assigned Tags"));
    m_allTagsModel->setHorizontalHeaderLabels(QStringList() << tr("All Tags"));
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
    foreach (const QModelIndex &index, ui->allTagsTreeView->selectionModel()->selectedRows())
        tags << index.data().toString();
    addTags(tags);
}

void TagsEditDialog::removeTags()
{
    QStringList tags;
    foreach (const QModelIndex &index, ui->objectTagsTreeView->selectionModel()->selectedRows())
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
    foreach (const QString &tag, tags)
    {
        if (tag.isEmpty())
            continue;

        {
            QList<QStandardItem *> items = m_objectTagsModel->findItems(tag);
            QStandardItem *item = !items.isEmpty() ? items.first() : new QStandardItem(tag);
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            if (items.isEmpty())
                m_objectTagsModel->appendRow(item);
        }

        {
            QList<QStandardItem *> items = m_allTagsModel->findItems(tag);
            QStandardItem *item = !items.isEmpty() ? items.first() : new QStandardItem(tag);
            item->setFlags(Qt::NoItemFlags);
            if (items.isEmpty())
                m_allTagsModel->appendRow(item);
        }
    }

    m_objectTagsModel->sort(0);
    m_allTagsModel->sort(0);
}

void TagsEditDialog::removeTags(const QStringList &tags)
{
    foreach (const QString &tag, tags)
    {
        if (tag.isEmpty())
            continue;

        {
            QList<QStandardItem *> items = m_objectTagsModel->findItems(tag);
            QStandardItem *item = !items.isEmpty() ? items.first() : 0;
            if (!item || item->flags() == Qt::NoItemFlags)
                continue;

            qDeleteAll(m_objectTagsModel->takeRow(item->row()));
        }

        {
            QList<QStandardItem *> items = m_allTagsModel->findItems(tag);
            QStandardItem *item = !items.isEmpty() ? items.first() : 0;
            if (item)
                item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        }
    }

    m_objectTagsModel->sort(0);
    m_allTagsModel->sort(0);
}
