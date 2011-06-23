enum QnDomain 
{
    QnDomain_Memory = 1,
    QnDomain_Database = 2,
    QnDomain_Physical = 4
};

class QnResourceConsumer
{
public:
    QnResourceConsumer(QnResource*);
    
    virtual void beforeDisconnectFromResource()=0;
    void disconnect();

    QnResource* getResource() const;



};

class QnResource: QObject 
{
public:

    QnResource();
    virtual ~QnResource();

    void setParentID(const QnID& value);
    QnID getParentID() const;

    void setID(const QnID& value);
    QnID getID() const;

    void addTag(const QString& value);
    QString removeTag(const QString& value);
    bool hasTag(const QString& value) const;

    QString getName() const;
    void setName(const QString& value);

    virtual QnParameter getParamet(const QString& paramName) const = 0;
    virtual void setParam(const QnParameter& value, QnDomain domain = QnDomain_Memory) = 0;
    //QList<QString> getParamNames() const;
    //QList<QVariant> getParamValues() const;

    void QnParameter getParametAsync(const QString& paramName) const;
    void setParamAsync(const QnParameter& value, QnDomain domain = QnDomain_Memory);
signals:
    void onParameterChanged(const QString paramName, const QnParameter value);
private:
    int m_refCount;
    QnID m_id;
    QnID m_parentID;
    QString m_name;
};

class QnStreamResource 
{
public:
    enum StreamType {
        Stream_Primary,
        Stream_Secondary
    };
    QnStreamProvider* getStream(StreamType type);
private:
    m_primaryProvider;
    m_secondaryProvider;
};

class QVideoCamera: public QResource, public QnStreamResource
{
};

class QnStreamProvider: QnLongRunable
{
public:
    QnStreamProvider(QnResource* resource);
    enum StreamQuality {CLSLowest, CLSLow, CLSNormal, CLSHigh, CLSHighest};

    QnResource* getResource() const;

    virtual bool dataCanBeAccepted() const;

    void setStatistics(CLStatistics* stat);
    virtual void setStreamParams(CLParamList newParam);
    CLParamList getStreamParam() const;

    void addDataProcessor(CLAbstractDataProcessor* dp);
    void removeDataProcessor(CLAbstractDataProcessor* dp);

    void pauseDataProcessors()
    {
        foreach(CLAbstractDataProcessor* dataProcessor, m_dataprocessors) {
            dataProcessor->pause();
        }
    }

    void resumeDataProcessors()
    {
        foreach(CLAbstractDataProcessor* dataProcessor, m_dataprocessors) {
            dataProcessor->resume();
        }
    }

    void setRealTimeMode(bool value);

protected:
    QnDataPacket* getNextData() = 0;
};

class QnVideoStreamProvider: public QnStreamProvider 
{
    setStreamQuality(StreamQuality quality, float fps);
    virtual void setNeedKeyData(int channel = 0);
    virtual bool needKeyData(int channel = 0) const;
};

class QnDataPacket
{

};

class QnMediaPacket: public QnDataPacket
{

};

QnResourceController
{

};


{
    QnResource* someResource ;
    dynamic_cast<>
}
