// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QSet>
#include <QtWidgets/QWidget>

#include <core/resource/camera_bookmark_fwd.h>

namespace Ui { class BookmarkWidget; }

class QnBookmarkWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit QnBookmarkWidget(QWidget* parent = nullptr);
    virtual ~QnBookmarkWidget() override;

    const QnCameraBookmarkTagList& tags() const;
    void setTags(const QnCameraBookmarkTagList& tags);

    void loadData(const QnCameraBookmark& bookmark);
    void submitData(QnCameraBookmark& bookmark) const;

    /** Check if entered data is valid. */
    bool isValid() const;

    void setDescriptionMandatory(bool mandatory);

signals:
    bool validChanged();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void updateTagsList();

private:
    QScopedPointer<Ui::BookmarkWidget> ui;
    QnCameraBookmarkTagList m_allTags;
    QnCameraBookmarkTags m_selectedTags;
    bool m_isValid;
};
