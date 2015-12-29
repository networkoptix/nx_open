#ifndef BOOKMARK_WIDGET_H
#define BOOKMARK_WIDGET_H

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <core/resource/camera_bookmark_fwd.h>

namespace Ui {
    class BookmarkWidget;
}

class QnBookmarkWidget : public QWidget {
    Q_OBJECT

public:
    explicit QnBookmarkWidget(QWidget *parent = 0);
    ~QnBookmarkWidget();

    const QnCameraBookmarkTagList &tags() const;
    void setTags(const QnCameraBookmarkTagList &tags);

    void loadData(const QnCameraBookmark &bookmark);
    void submitData(QnCameraBookmark &bookmark) const;

private:
    void updateTagsList();

private:
    QScopedPointer<Ui::BookmarkWidget> ui;
    QnCameraBookmarkTagList m_allTags;
    QnCameraBookmarkTags m_selectedTags;
};

#endif // BOOKMARK_WIDGET_H
