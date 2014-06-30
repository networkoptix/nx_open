#ifndef single_shot_file_reader_h_215
#define single_shot_file_reader_h_215

#include "core/dataprovider/media_streamdataprovider.h"

// difference between this class and pull reader is that run function does not have infinit loop
// it quits after first getData
// such reader can be used for photo 

class QnSingleShotFileStreamreader : public QnAbstractMediaStreamDataProvider
{
public:
    QnSingleShotFileStreamreader(const QnResourcePtr& resource);
    ~QnSingleShotFileStreamreader(){stop();}
    void setStorage(const QnStorageResourcePtr& storage);

    //!Implementation of QnAbstractMediaStreamDataProvider::diagnoseMediaStreamConnection
    virtual CameraDiagnostics::Result diagnoseMediaStreamConnection() override;

protected:
    virtual QnAbstractMediaDataPtr getNextData();
    virtual void run() override;
private:
    QnStorageResourcePtr m_storage;
};

/**/

#endif //single_shot_file_reader_h_215
