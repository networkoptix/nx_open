import { NgModule } from '@angular/core';

import { SandboxModule } from './sandbox/sandbox.module';

import { MainModule }                from './main/main.module';
import { DownloadModule }            from './download/download.module';
import { DownloadHistoryModule }     from './download-history/download-history.module';
import { NonSupportedBrowserModule } from './non-supported-browser/non-supported-browser.module';
import { ServersDetailModule }       from './details/servers/servers.module';
import { UsersDetailModule }         from './details/users/users.module';
import { OtherDetailsModule }        from './details/others/others.module';

@NgModule({
    imports: [
        SandboxModule,
        MainModule,
        DownloadModule,
        DownloadHistoryModule,
        NonSupportedBrowserModule,
        ServersDetailModule,
        UsersDetailModule,
        OtherDetailsModule
    ],
    declarations: [],
    entryComponents: [],
    providers: [],
    exports: [
        SandboxModule,
        MainModule,
        DownloadModule,
        DownloadHistoryModule,
        NonSupportedBrowserModule,
        ServersDetailModule,
        UsersDetailModule,
        OtherDetailsModule
    ]
})
export class PagesModule {
}
