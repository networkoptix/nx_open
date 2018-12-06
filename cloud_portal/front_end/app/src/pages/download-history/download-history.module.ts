import { NgModule }                          from '@angular/core';
import { CommonModule }                      from '@angular/common';
import { BrowserModule }                     from '@angular/platform-browser';
import { downgradeComponent, UpgradeModule } from '@angular/upgrade/static';
import { RouterModule, Routes }              from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { ComponentsModule }         from '../../components/components.module';
import { ReleaseComponent }         from './release/release.component';
import { DownloadHistoryComponent } from './download-history.component';
import { TranslateModule }          from '@ngx-translate/core';

const appRoutes: Routes = [
    {path: 'downloads/history', component: DownloadHistoryComponent},
    {path: 'downloads/:type', component: DownloadHistoryComponent}
];

@NgModule({
    imports: [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        NgbModule,
        TranslateModule,

        ComponentsModule
        // RouterModule.forChild(appRoutes)
    ],
    providers: [],
    declarations: [
        DownloadHistoryComponent,
        ReleaseComponent
    ],
    bootstrap: [],
    entryComponents: [
        DownloadHistoryComponent
    ],
    exports: [
        DownloadHistoryComponent
    ]
})
export class DownloadHistoryModule {
}

declare var angular: angular.IAngularStatic;
angular
    .module('cloudApp.directives')
    .directive('downloadHistory', downgradeComponent({component: DownloadHistoryComponent}) as angular.IDirectiveFactory);
