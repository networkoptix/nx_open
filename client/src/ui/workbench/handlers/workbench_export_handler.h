#ifndef WORKBENCH_EXPORT_HANDLER_H
#define WORKBENCH_EXPORT_HANDLER_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>


/**
 * @brief The QnWorkbenchExportHandler class            Handler for video and layout export related actions.
 */
class QnWorkbenchExportHandler: public QObject, protected QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QObject base_type;
public:
    QnWorkbenchExportHandler(QObject *parent = NULL);

private:
#ifdef Q_OS_WIN
    QString binaryFilterName() const;
#endif

private slots:
    void at_exportTimeSelectionAction_triggered();
};

#endif // WORKBENCH_EXPORT_HANDLER_H
