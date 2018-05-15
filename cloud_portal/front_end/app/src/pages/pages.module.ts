import { NgModule } from '@angular/core';

import { DownloadModule }        from './download/download.module';
import { DownloadHistoryModule } from './download-history/download-history.module';

@NgModule({
    imports: [
        DownloadModule,
        DownloadHistoryModule
    ],
    declarations: [],
    entryComponents: [],
    providers: [],
    exports: [
        DownloadModule,
        DownloadHistoryModule
    ]
})
export class PagesModule {
}
