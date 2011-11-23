#ifndef QnExternalTimeSource_h_1919
#define QnExternalTimeSource_h_1919

class QnlTimeSource
{
public:
    virtual qint64 getCurrentTime() const = 0;
    virtual qint64 getNextTime() const = 0;
};

#endif //QnExternalTimeSource_h_1919