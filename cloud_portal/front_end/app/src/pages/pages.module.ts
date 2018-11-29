import { NgModule } from '@angular/core';

import { SandboxModule } from './sandbox/sandbox.module';
import { CampageModule } from './campage/campage.module';
import { DropdownsModule } from '../dropdowns/dropdowns.module';
import { Angular2CsvModule } from 'angular2-csv';

import { MainModule }                from './main/main.module';
import { DownloadModule }            from './download/download.module';
import { DownloadHistoryModule }     from './download-history/download-history.module';
import { NonSupportedBrowserModule } from './non-supported-browser/non-supported-browser.module';
import { ServersDetailModule }       from './details/servers/servers.module';
import { UsersDetailModule }         from './details/users/users.module';
import { OtherDetailsModule }        from './details/others/others.module';

import { RightMenuModule }        from './right-menu/right-menu.module';
import { ContentModule }          from './content/content.module';
import { IntegrationsModule }     from './integration/integrations.module';
import { IntegrationsListModule } from './integration//list/list.module';

@NgModule({
    imports        : [
        SandboxModule,
        MainModule,
        DownloadModule,
        DownloadHistoryModule,
        NonSupportedBrowserModule,
        ServersDetailModule,    // TODO: Remove it after test
        UsersDetailModule,      // TODO: Remove it after test
        OtherDetailsModule,     // TODO: Remove it after test
        IntegrationsModule,
        IntegrationsListModule,
        ContentModule,          // TODO: Remove it after test
        RightMenuModule,        // TODO: Remove it after test
        CampageModule,
        DropdownsModule,
        Angular2CsvModule,
    ],
    declarations   : [],
    entryComponents: [],
    providers      : [],
    exports        : [
        SandboxModule,
        MainModule,
        DownloadModule,
        DownloadHistoryModule,
        NonSupportedBrowserModule,
        ServersDetailModule,    // TODO: Remove it after test
        UsersDetailModule,      // TODO: Remove it after test
        OtherDetailsModule,     // TODO: Remove it after test
        IntegrationsModule,
        IntegrationsListModule,
        ContentModule,          // TODO: Remove it after test
        RightMenuModule,        // TODO: Remove it after test
        CampageModule,
        DropdownsModule,
        Angular2CsvModule,
    ]
})
export class PagesModule {
}
