import { NgModule } from '@angular/core';

import { DownloadModule }               from './download/download.module';
import { DownloadHistoryModule }        from './download-history/download-history.module';
import { NonSupportedBrowserModule }    from "./non-supported-browser/non-supported-browser.module";

@NgModule({
    imports: [
        DownloadModule,
        DownloadHistoryModule,
        NonSupportedBrowserModule
    ],
    declarations: [],
    entryComponents: [],
    providers: [],
    exports: [
        DownloadModule,
        DownloadHistoryModule,
        NonSupportedBrowserModule
    ]
})
export class PagesModule {
}
