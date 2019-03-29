import { NgModule }                          from '@angular/core';
import { CommonModule }                      from '@angular/common';
import { BrowserModule }                     from '@angular/platform-browser';
import { downgradeComponent, UpgradeModule } from '@angular/upgrade/static';
import { RouterModule, Routes }              from '@angular/router';

import { NxLandingComponent } from './landing.component';

import { TranslateModule }   from '@ngx-translate/core';
import { ComponentsModule }  from '../../components/components.module';
import { DownloadComponent } from '../download/download.component';

const appRoutes: Routes = [
    { path    : '', component: NxLandingComponent },
    { path    : 'login', component: NxLandingComponent }
];

@NgModule({
    imports        : [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        TranslateModule,
        ComponentsModule,

        // RouterModule.forChild(appRoutes)
    ],
    providers      : [],
    declarations   : [
        NxLandingComponent,
    ],
    bootstrap      : [],
    entryComponents: [
        NxLandingComponent
    ],
    exports        : [
        NxLandingComponent
    ]
})
export class LandingModule {
}

declare var angular: angular.IAngularStatic;
angular
        .module('cloudApp.directives')
        .directive('landingComponent', downgradeComponent({ component: NxLandingComponent }) as angular.IDirectiveFactory);

