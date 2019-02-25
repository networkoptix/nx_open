import { NgModule }                  from '@angular/core';
import { CommonModule }              from '@angular/common';
import { NxLanguageProviderService } from './nx-language-provider';
import { NxConfigService }           from './nx-config';
import { downgradeInjectable }       from '@angular/upgrade/static';


@NgModule({
    imports        : [
        CommonModule,
    ],
    declarations   : [
    ],
    entryComponents: [
    ],
    providers      : [
        NxLanguageProviderService,
        NxConfigService
    ],
    exports        : []
})
export class ServiceModule {
}

declare var angular: angular.IAngularStatic;
angular
    .module('cloudApp.services')
    .service('nxLanguageService', downgradeInjectable(NxLanguageProviderService))
    .service('nxConfigService', downgradeInjectable(NxConfigService));

