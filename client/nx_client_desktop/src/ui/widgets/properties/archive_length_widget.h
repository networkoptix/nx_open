#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <utils/common/updatable.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class ArchiveLengthWidget; }

class QnAligner;

class QnArchiveLengthWidget: public QWidget, public QnUpdatable, public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    using base_type = QWidget;

public:
    explicit QnArchiveLengthWidget(QWidget* parent = nullptr);
    virtual ~QnArchiveLengthWidget();

    QnAligner* aligner() const;

    void updateFromResources(const QnVirtualCameraResourceList& cameras);
    void submitToResources(const QnVirtualCameraResourceList& cameras);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    QString alert() const;
    void setAlert(const QString& alert);

    int maxRecordedDays() const;
    int minRecordedDays() const;

signals:
    void alertChanged(const QString& text);
    void changed();

private:
    void validateArchiveLength();
    void updateArchiveRangeEnabledState();
    void updateMinDays(const QnVirtualCameraResourceList& cameras);
    void updateMaxDays(const QnVirtualCameraResourceList& cameras);

private:
    QScopedPointer<Ui::ArchiveLengthWidget> ui;
    bool m_readOnly = false;
    QString m_alert;
    QnAligner* m_aligner = nullptr;
};
