#include "workbench_screenshot_handler.h"

#include <QtGui/QImageWriter>
#include <QtGui/QPainter>

#include <QtWidgets/QAction>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QMessageBox>

#include <camera/cam_display.h>

#include <client/client_settings.h>

#include <ui/actions/action_manager.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/workbench/workbench_item.h>

#include <utils/common/string.h>
#include <utils/common/warnings.h>

#include "file_processor.h"

QnWorkbenchScreenshotHandler::QnWorkbenchScreenshotHandler(QObject *parent): 
    QObject(parent), 
    QnWorkbenchContextAware(parent) 
{
    connect(action(Qn::TakeScreenshotAction), SIGNAL(triggered()), this, SLOT(at_takeScreenshotAction_triggered()));
}

void QnWorkbenchScreenshotHandler::drawTimeStamp(QPainter *painter, const QRect &rect, Qn::Corner position, const QString &timestamp) {
    if (position == Qn::NoCorner)
        return;

    QFont font;
    font.setPixelSize(qMax(rect.height() / 20, 12));

    QFontMetrics fm(font);
    QSize size = fm.size(0, timestamp);
    int spacing = 2;

    painter->setPen(Qt::black);
    painter->setBrush(Qt::white);
    painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::HighQualityAntialiasing);

    QPainterPath path;
    int x = rect.x();
    int y = rect.y();

    switch (position) {
    case Qn::TopLeftCorner:
    case Qn::BottomLeftCorner:
        x += rect.x() + spacing * 2;
        break;
    default:
        x += rect.width() - size.width() - spacing*2;
        break;
    }

    switch (position) {
    case Qn::TopLeftCorner:
    case Qn::TopRightCorner:
        y += spacing + fm.ascent();
        break;
    default:
        y += rect.height() - fm.descent() - spacing;
        break;
    }

    path.addText(x, y, font, timestamp);
    painter->drawPath(path);
}

void QnWorkbenchScreenshotHandler::at_takeScreenshotAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(parameters.widget());
    if (!widget)
        return;

    QnResourceDisplayPtr display = widget->display();
    QnConstResourceVideoLayoutPtr layout = display->videoLayout();

    // TODO: #Elric move out, common code
    QString timeString;
    qint64 time = display->camDisplay()->getCurrentTime() / 1000;
    if(widget->resource()->toResource()->flags() & QnResource::utc) {
        timeString = QDateTime::fromMSecsSinceEpoch(time).toString(lit("yyyy-MMM-dd_hh.mm.ss"));
    } else {
        timeString = QTime().addMSecs(time).toString(lit("hh.mm.ss"));
    }

    if (layout->channelCount() == 0) {
        qnWarning("No channels in resource '%1' of type '%2'.", widget->resource()->toResource()->getName(), widget->resource()->toResource()->metaObject()->className());
        return;
    }

    QString fileName = parameters.argument<QString>(Qn::FileNameRole);
    Qn::Corner timestampPos = qnSettings->timestampCorner();
    if(fileName.isEmpty()) {
        QString suggestion = replaceNonFileNameCharacters(widget->resource()->toResource()->getName(), QLatin1Char('_')) + QLatin1Char('_') + timeString;

        QString previousDir = qnSettings->lastScreenshotDir();
        if (previousDir.isEmpty())
            previousDir = qnSettings->mediaFolder();

        QString filterSeparator(QLatin1String(";;"));
        QString pngFileFilter = tr("PNG Image (*.png)");
        QString jpegFileFilter = tr("JPEG Image(*.jpg)");

        QString allowedFormatFilter =
            pngFileFilter;

        if (QImageWriter::supportedImageFormats().contains("jpg") ) {
            allowedFormatFilter += filterSeparator;
            allowedFormatFilter += jpegFileFilter;
        }

        QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(
            mainWindow(),
            tr("Save Screenshot As..."),
            previousDir + QDir::separator() + suggestion,
            allowedFormatFilter
        ));

        dialog->setFileMode(QFileDialog::AnyFile);
        dialog->setAcceptMode(QFileDialog::AcceptSave);

        QComboBox* comboBox = new QComboBox(dialog.data());
        comboBox->addItem(tr("No timestamp"), Qn::NoCorner);
        comboBox->addItem(tr("Top left corner"), Qn::TopLeftCorner);
        comboBox->addItem(tr("Top right corner"), Qn::TopRightCorner);
        comboBox->addItem(tr("Bottom left corner"), Qn::BottomLeftCorner);
        comboBox->addItem(tr("Bottom right corner"), Qn::BottomRightCorner);
        
        for (int i = 0; i < comboBox->count(); ++i) {
            if (comboBox->itemData(i) == timestampPos)
                comboBox->setCurrentIndex(i);
        }

        QLabel* label = new QLabel(dialog.data());
        label->setText(tr("Timestamps:"));

        dialog->addWidget(label);
        dialog->addWidget(comboBox, false);


        if (!dialog->exec() || dialog->selectedFiles().isEmpty())
            return;

        fileName = dialog->selectedFiles().value(0);
        if (fileName.isEmpty())
            return;

        QString selectedFilter = dialog->selectedNameFilter();
        QString selectedExtension = selectedFilter.mid(selectedFilter.lastIndexOf(QLatin1Char('.')), 4);
        if (!fileName.toLower().endsWith(selectedExtension)) {
            fileName += selectedExtension;

            if (QFile::exists(fileName)) {
                QMessageBox::StandardButton button = QMessageBox::information(
                    mainWindow(),
                    tr("Save As"),
                    tr("File '%1' already exists. Do you want to replace it?").arg(QFileInfo(fileName).baseName()),
                    QMessageBox::Ok | QMessageBox::Cancel
                );
                if(button == QMessageBox::Cancel)
                    return;
            }
        }

        if (QFile::exists(fileName) && !QFile::remove(fileName)) {
            QMessageBox::critical(
                mainWindow(),
                tr("Could not overwrite file"),
                tr("File '%1' is used by another process. Please try another name.").arg(QFileInfo(fileName).baseName()),
                QMessageBox::Ok
            );
            return;
        }
        timestampPos = (Qn::Corner) comboBox->itemData(comboBox->currentIndex()).toInt();
    }

    QImage screenshot;
    {
        QList<QImage> images;
        QnItemDewarpingParams itemDewarpingParams;
        QnMediaDewarpingParams mediaDewarpingParams = widget->dewarpingParams();
        if (mediaDewarpingParams.enabled)
            itemDewarpingParams = widget->itemDewarpingParams();
        for (int i = 0; i < layout->channelCount(); ++i)
            images.push_back(display->camDisplay()->getScreenshot(i, widget->item()->imageEnhancement(), mediaDewarpingParams, itemDewarpingParams));
        QSize channelSize = images[0].size();
        QSize totalSize = QnGeometry::cwiseMul(channelSize, layout->size());
        QRectF zoomRect = (widget->zoomRect().isNull() || mediaDewarpingParams.enabled) ? QRectF(0, 0, 1, 1) : widget->zoomRect();

        screenshot = QImage(totalSize.width() * zoomRect.width(), totalSize.height() * zoomRect.height(), QImage::Format_ARGB32);
        screenshot.fill(qRgba(0, 0, 0, 0));
        QScopedPointer<QPainter> painter(new QPainter(&screenshot));
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        for (int i = 0; i < layout->channelCount(); ++i) {
            painter->drawImage(
                QnGeometry::cwiseMul(layout->position(i), channelSize) - QnGeometry::cwiseMul(zoomRect.topLeft(), totalSize),
                images[i]
            );
        }

        drawTimeStamp(painter.data(), screenshot.rect(), timestampPos, timeString);
    }

    if (!screenshot.save(fileName)) {
        QMessageBox::critical(
            mainWindow(),
            tr("Could not save screenshot"),
            tr("An error has occurred while saving screenshot '%1'.").arg(QFileInfo(fileName).baseName())
        );
        return;
    }
    
    QnFileProcessor::createResourcesForFile(fileName);
    
    qnSettings->setLastScreenshotDir(QFileInfo(fileName).absolutePath());
    qnSettings->setTimestampCorner(timestampPos);
}
