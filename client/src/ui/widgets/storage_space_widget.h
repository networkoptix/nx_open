#ifndef STORAGE_SPACE_WIDGET_H
#define STORAGE_SPACE_WIDGET_H

#include <QtWidgets/QWidget>
#include <QtWidgets/QSlider>

class QnStorageSpaceWidget : public QSlider
{
    Q_OBJECT
    typedef QSlider base_type;
    
public:
    explicit QnStorageSpaceWidget(QWidget *parent = 0);
    ~QnStorageSpaceWidget();

    void setLockedValue(int value) { m_lockedValue = value; }
    int lockedValue() const { return m_lockedValue; }

    void setRecordedValue(int value) { m_recordedValue = value; }
    int recordedValue() const { return m_recordedValue; }

protected:
    void sliderChange(SliderChange change) override;

    /** ReImp */
    void paintEvent(QPaintEvent *ev);

private:
    int m_lockedValue;
    int m_recordedValue;
};

#endif // STORAGE_SPACE_WIDGET_H
