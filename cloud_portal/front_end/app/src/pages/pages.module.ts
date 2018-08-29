import { NgModule } from '@angular/core';

import { SandboxModule } from './sandbox/sandbox.module';

import { MainModule }                from './main/main.module';
import { DownloadModule }            from './download/download.module';
import { DownloadHistoryModule }     from './download-history/download-history.module';
import { NonSupportedBrowserModule } from './non-supported-browser/non-supported-browser.module';

@NgModule({
    imports: [
        SandboxModule,
        MainModule,
        DownloadModule,
        DownloadHistoryModule,
        NonSupportedBrowserModule
    ],
    declarations: [],
    entryComponents: [],
    providers: [],
    exports: [
        SandboxModule,
        MainModule,
        DownloadModule,
        DownloadHistoryModule,
        NonSupportedBrowserModule
    ]
})
export class PagesModule {
}
