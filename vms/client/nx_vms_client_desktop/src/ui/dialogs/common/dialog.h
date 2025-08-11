// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QDialog>

template<class T, Qt::WindowModality modality = Qt::ApplicationModal, class... Args>
T* createSelfDestructingDialog(Args... args)
{
    T* dialog = new T{std::forward<Args>(args)...};
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(modality);

    return dialog;
}

/// @brief Base dialog class. Fixes dialog hang on when dragging items on show.
/// Cancels drag event before showing dialog.
class QnDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(Qt::Orientations resizeToContentsMode READ resizeToContentsMode WRITE setResizeToContentsMode)
    using base_type = QDialog;

public:
    QnDialog(QWidget* parent, Qt::WindowFlags flags = {});
    virtual ~QnDialog();

    /// @brief Shows dialog and cancels drag action to prevent hang on
    void show();

    virtual void open() override;

    [[deprecated]] int exec() override;

    Qt::Orientations resizeToContentsMode() const;
    void setResizeToContentsMode(Qt::Orientations mode);

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

    /* Safe minimum size which wont override minimumSizeHint(): */
    void setSafeMinimumWidth(int width);
    int safeMinimumWidth() const;
    void setSafeMinimumHeight(int height);
    int safeMinimumHeight() const;
    void setSafeMinimumSize(const QSize& size);
    QSize safeMinimumSize() const;

protected:
    virtual bool event(QEvent* event) override;
    virtual void afterLayout();

private:
    void fixWindowFlags();

private:
    Qt::Orientations m_resizeToContentsMode;
    QSize m_safeMinimumSize;
};
