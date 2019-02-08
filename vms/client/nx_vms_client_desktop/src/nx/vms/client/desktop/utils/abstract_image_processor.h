#pragma once

#include <QtCore/QObject>
#include <QtGui/QImage>

namespace nx::vms::client::desktop {

class AbstractImageProcessor: public QObject
{
   Q_OBJECT
   using base_type = QObject;

public:
   explicit AbstractImageProcessor(QObject* parent = nullptr);
   virtual ~AbstractImageProcessor() = default;

   virtual QSize process(const QSize& sourceSize) const = 0;
   virtual QImage process(const QImage& sourceImage) const = 0;

signals:
   void updateRequired();
};

} // namespace nx::vms::client::desktop
