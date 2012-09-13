#include "tags_edit_dialog.h"
#include "ui_tags_edit_dialog.h"

#include <QtCore/QSet>

#include <QtGui/QSortFilterProxyModel>
#include <QtGui/QStandardItemModel>

#include "core/resource/resource.h"
#include "core/resourcemanagment/resource_pool.h"

#include "ui/style/skin.h"

TagsEditDialog::TagsEditDialog(const QStringList &objectIds, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TagsEditDialog),
    m_objectIds(objectIds)
{
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

    // TODO
    ui->addTagsButton->setIcon(qnSkin->icon("left-arrow.png"));
    ui->removeTagsButton->setIcon(qnSkin->icon("right-arrow.png"));
    ui->addTagButton->setIcon(qnSkin->icon("plus.png"));
    ui->clearFilterButton->setIcon(qnSkin->icon("clear.png"));

    connect(ui->tagsFilterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(filterChanged(QString)));
    connect(ui->clearFilterButton, SIGNAL(clicked()), ui->tagsFilterLineEdit, SLOT(clear()));

    connect(ui->newTagLineEdit, SIGNAL(returnPressed()), ui->addTagButton, SLOT(animateClick()));
    connect(ui->addTagButton, SIGNAL(clicked()), this, SLOT(addTag()));
    connect(ui->addTagsButton, SIGNAL(clicked()), this, SLOT(addTags()));
    connect(ui->removeTagsButton, SIGNAL(clicked()), this, SLOT(removeTags()));

    if (QPushButton *button = ui->buttonBox->button(QDialogButtonBox::RestoreDefaults))
        connect(button, SIGNAL(clicked()), this, SLOT(reset()));

    reset();

    ui->newTagLineEdit->setFocus();
}

TagsEditDialog::~TagsEditDialog()
{
}

void TagsEditDialog::accept()
{
    QSet<QString> oldIntersectedObjectTagsSet;
    if (!m_objectIds.isEmpty()) {
        QnResourcePtr resource = qnResPool->getResourceByUniqId(m_objectIds.at(0));
        if (resource) {
            oldIntersectedObjectTagsSet = resource->tagList().toSet();
            for (int i = 1; i < m_objectIds.size(); ++i) {
                resource = qnResPool->getResourceByUniqId(m_objectIds.at(i));
                if (resource)
                    oldIntersectedObjectTagsSet = oldIntersectedObjectTagsSet.intersect(resource->tagList().toSet());
            }
        }
    }

    QSet<QString> intersectedObjectTagsSet;
    for (int row = 0, rowCount = m_objectTagsModel->rowCount(); row < rowCount; ++row) {
        QStandardItem *item = m_objectTagsModel->item(row);
        if (item->flags() != Qt::NoItemFlags)
            intersectedObjectTagsSet.insert(item->text());
    }

    foreach (const QString &objectId, m_objectIds) {
        QnResourcePtr resource = qnResPool->getResourceByUniqId(objectId);
        if (resource) {
            QSet<QString> oldObjectTags = resource->tagList().toSet();
            QSet<QString> objectTags = oldObjectTags;
            if (!oldIntersectedObjectTagsSet.isEmpty())
                objectTags = objectTags.subtract(oldIntersectedObjectTagsSet.subtract(intersectedObjectTagsSet));
            if (!intersectedObjectTagsSet.isEmpty())
                objectTags = objectTags.unite(intersectedObjectTagsSet.subtract(oldIntersectedObjectTagsSet));
            if (objectTags != oldObjectTags)
                resource->setTags(objectTags.toList());
        }
    }

    QDialog::accept();
}

void TagsEditDialog::reset()
{
    m_objectTagsModel->clear();
    m_allTagsModel->clear();

    retranslateUi();

    QSet<QString> intersectedObjectTagsSet;
    QSet<QString> allObjectTagsSet;
    if (!m_objectIds.isEmpty()) {
        QnResourcePtr resource = qnResPool->getResourceByUniqId(m_objectIds.at(0));
        if (resource) {
            allObjectTagsSet = intersectedObjectTagsSet = resource->tagList().toSet();
            for (int i = 1; i < m_objectIds.size(); ++i) {
                resource = qnResPool->getResourceByUniqId(m_objectIds.at(i));
                if (resource) {
                    QSet<QString> objectTags = resource->tagList().toSet();
                    intersectedObjectTagsSet = intersectedObjectTagsSet.intersect(objectTags);
                    allObjectTagsSet = allObjectTagsSet.unite(objectTags);
                }
            }
        }
        QStringList allObjectTags = allObjectTagsSet.toList();
        qSort(allObjectTags);
        foreach (const QString &tag, allObjectTags) {
            QStandardItem *item = new QStandardItem(tag);
            if (intersectedObjectTagsSet.contains(tag))
                item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            else
                item->setFlags(Qt::NoItemFlags);
            m_objectTagsModel->appendRow(item);
        }
    }

    QStringList allTags = qnResPool->allTags();
    qSort(allTags);
    foreach (const QString &tag, allTags) {
        QStandardItem *item = new QStandardItem(tag);
        if (!intersectedObjectTagsSet.contains(tag))
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        else
            item->setFlags(Qt::NoItemFlags);
        m_allTagsModel->appendRow(item);
    }
}

void TagsEditDialog::retranslateUi()
{
    m_objectTagsModel->setHorizontalHeaderLabels(QStringList() << tr("Assigned Tags"));
    m_allTagsModel->setHorizontalHeaderLabels(QStringList() << tr("All Tags"));
}

void TagsEditDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);

    switch (event->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        retranslateUi();
        break;
    default:
        break;
    }
}

void TagsEditDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        return;

    QDialog::keyPressEvent(event);
}

void TagsEditDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    if (ui->newTagLineEdit->height() != ui->tagsFilterLineEdit->height()) {
        const int maxHeight = qMax(ui->newTagLineEdit->height(), ui->tagsFilterLineEdit->height());
        ui->newTagLineEdit->setMaximumHeight(maxHeight);
        ui->tagsFilterLineEdit->setMaximumHeight(maxHeight);
        if (ui->addTagButton->height() > maxHeight)
            ui->addTagButton->setMaximumSize(maxHeight, maxHeight);
        if (ui->clearFilterButton->height() > maxHeight)
            ui->clearFilterButton->setMaximumSize(maxHeight, maxHeight);
    }
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
    if (!filter.isEmpty()) {
        ui->clearFilterButton->show();
        m_allTagsProxyModel->setFilterWildcard(QLatin1Char('*') + filter + QLatin1Char('*'));
    } else {
        ui->clearFilterButton->hide();
        m_allTagsProxyModel->invalidate();
    }
}

void TagsEditDialog::addTags(const QStringList &tags)
{
    foreach (const QString &tag, tags) {
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
    foreach (const QString &tag, tags) {
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
