// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef PTZ_TOURS_DIALOG_H
#define PTZ_TOURS_DIALOG_H

#include <core/resource/resource_fwd.h>
#include <core/ptz/ptz_fwd.h>

#include <ui/dialogs/abstract_ptz_dialog.h>
#include <ui/models/ptz_manage_model.h>
#include <nx/utils/singleton.h>

namespace Ui {
    class PtzManageDialog;
}

namespace nx::vms::client::desktop {

class LocalFileCache;

} // namespace nx::vms::client::desktop


class QnPtzManageModel;
class QnHotkeysResourcePropertyAdaptor;
class QnAbstractPtzHotkeyDelegate;
class QnMediaResourceWidget;

// TODO: #sivanov Remove singleton.
class QnPtzManageDialog : public QnAbstractPtzDialog, public Singleton<QnPtzManageDialog> {
    Q_OBJECT
    typedef QnAbstractPtzDialog base_type;

public:
    explicit QnPtzManageDialog(QWidget *parent = 0);
    ~QnPtzManageDialog();

    QPointer<QnMediaResourceWidget> widget() const;
    void setWidget(QnMediaResourceWidget* widget);

    bool isModified() const;

    virtual bool tryClose(bool force) override;

    bool askToSaveChanges(bool cancelIsAllowed = true);

    /**
     * Note: dialog will take ownership of delegate.
     */
    void setHotkeysDelegate(QnAbstractPtzHotkeyDelegate* hotkeysDelegate);

protected:
    virtual void loadData(const QnPtzData &data) override;
    virtual void saveData() override;
    virtual DataFields requiredFields() const override;
    virtual void updateFields(DataFields fields) override;

    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void reject() override;
    virtual void accept() override;

private slots:
    void updateUi();
    void updateHotkeys();

    void at_tableView_currentRowChanged(const QModelIndex &current, const QModelIndex &previous);

    void at_savePositionButton_clicked();
    void at_goToPositionButton_clicked();
    void at_addTourButton_clicked();
    void at_startTourButton_clicked();
    void at_deleteButton_clicked();
    void at_getPreviewButton_clicked();

    void at_tableViewport_resizeEvent();
    void at_tourSpotsChanged(const QnPtzTourSpotList &spots);

    void at_cache_imageLoaded(const QString &filename);

    void storePreview(const QString &id);
    void setPreview(const QImage &image);

    void at_model_modelAboutToBeReset();
    void at_model_modelReset();

private:
    bool isValid() const;
    void updateCanSaveCurrentPosition();

    bool savePresets();
    bool saveTours();
    bool saveHomePosition();
    void enableDewarping();
    void clear();
    void setupTableChangesConfirmation();

private:
    QScopedPointer<Ui::PtzManageDialog> ui;

    QnPtzManageModel *m_model;
    QnAbstractPtzHotkeyDelegate* m_hotkeysDelegate;
    QPointer<QnMediaResourceWidget> m_widget;

    nx::vms::client::desktop::LocalFileCache *m_cache;
    QSet<QString> m_pendingPreviews;

    QnPtzManageModel::RowData m_lastRowData;
    int m_lastColumn;

    QString m_currentTourId;
    bool m_submitting;
};

#endif // PTZ_TOURS_DIALOG_H
