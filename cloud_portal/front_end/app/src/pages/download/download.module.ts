import { Injectable, NgModule }                  from '@angular/core';
import { CommonModule }                          from '@angular/common';
import { BrowserModule }                         from '@angular/platform-browser';
import { downgradeComponent, UpgradeModule } from '@angular/upgrade/static';
import { Router, Resolve, RouterModule, Routes } from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { DownloadComponent }          from './download.component';
import { Observable, EMPTY as empty } from "rxjs";
import { DeviceDetectorService }      from "ngx-device-detector";

@Injectable()
export class OsResolver implements Resolve<Observable<string>> {

    deviceInfo: any;
    platform: string;
    platformMatch: {};

    constructor(private router: Router,
                private deviceService: DeviceDetectorService) {

        this.deviceInfo = this.deviceService.getDeviceInfo();

        this.platformMatch = {
            'unix': 'Linux',
            'linux': 'Linux',
            'mac': 'MacOS',
            'windows': 'Windows'
        };
    }

    resolve() {
        this.platform = this.platformMatch[this.deviceInfo.os];
        if (this.platform) {
            this.router.navigate(["/download/" + this.platform]);
            return empty;
        }

        return this.deviceInfo;
    }
}

const appRoutes: Routes = [
    // {path: 'downloads', component: DownloadComponent},
    {path: 'download', component: DownloadComponent, resolve: {platform: OsResolver}},
    {path: 'download/:platform', component: DownloadComponent}
];

@NgModule({
    imports: [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        NgbModule,

        //RouterModule.forChild(appRoutes)
    ],
    providers: [
        OsResolver
    ],
    declarations: [
        DownloadComponent,
    ],
    bootstrap: [],
    entryComponents:[
        DownloadComponent
    ],
    exports:[
        DownloadComponent
    ]
})
export class DownloadModule {
}

declare var angular: angular.IAngularStatic;
angular
        .module('cloudApp.directives')
        .directive('downloadComponent', downgradeComponent({ component: DownloadComponent }) as angular.IDirectiveFactory)

