#ifndef TAGSEDITDIALOG_H
#define TAGSEDITDIALOG_H

#include <QtCore/QStringList>

#include <QtGui/QDialog>

class QSortFilterProxyModel;
class QStandardItemModel;

namespace Ui {
    class TagsEditDialog;
}

class TagsEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TagsEditDialog(const QStringList &objectIds, QWidget *parent = 0);
    virtual ~TagsEditDialog();

public Q_SLOTS:
    virtual void accept() override;
    void reset();

protected:
    virtual void changeEvent(QEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void showEvent(QShowEvent *event) override;

    void retranslateUi();

private Q_SLOTS:
    void addTag();
    void addTags();
    void removeTags();

    void filterChanged(const QString &filter);

private:
    void addTags(const QStringList &tags);
    void removeTags(const QStringList &tags);

private:
    Q_DISABLE_COPY(TagsEditDialog)

    QScopedPointer<Ui::TagsEditDialog> ui;

    QStringList m_objectIds;

    QStandardItemModel *m_objectTagsModel;
    QStandardItemModel *m_allTagsModel;
    QSortFilterProxyModel *m_allTagsProxyModel;
};

#endif // TAGSEDITDIALOG_H
