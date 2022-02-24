// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef single_shot_file_reader_h_215
#define single_shot_file_reader_h_215

#include "nx/streaming/abstract_media_stream_data_provider.h"

// difference between this class and pull reader is that run function does not have infinit loop
// it quits after first getData
// such reader can be used for photo

class QnStoragePluginFactory;

class QnSingleShotFileStreamreader: public QnAbstractMediaStreamDataProvider
{
public:
    QnSingleShotFileStreamreader(
        const QnResourcePtr& resource,
        QnStoragePluginFactory* storageFactory);

    ~QnSingleShotFileStreamreader(){stop();}
    void setStorage(const QnStorageResourcePtr& storage);

    //!Implementation of QnAbstractMediaStreamDataProvider::diagnoseMediaStreamConnection
    virtual CameraDiagnostics::Result diagnoseMediaStreamConnection() override;

protected:
    virtual QnAbstractMediaDataPtr getNextData();
    virtual void run() override;

private:
    QnStorageResourcePtr m_storage;
    QnStoragePluginFactory* const m_storageFactory;
};


#endif //single_shot_file_reader_h_215
