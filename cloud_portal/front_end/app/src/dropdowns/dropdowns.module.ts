import { NgModule }     from '@angular/core';
import { CommonModule } from '@angular/common';

import { NgbModalModule, NgbDropdownModule } from '@ng-bootstrap/ng-bootstrap';
import { NxAccountSettingsDropdown }         from "../dropdowns/account-settings/account-settings.component";
import { NxLanguageDropdown }                from "../dropdowns/language/language.component";
import { NxActiveSystemDropdown }            from "../dropdowns/active-system/active-system.component";
import { NxSystemsDropdown }                 from "../dropdowns/systems/systems.component";
import { downgradeComponent }                from "@angular/upgrade/static";

@NgModule({
    imports: [
        CommonModule,

        NgbDropdownModule,
        NgbModalModule,
    ],
    declarations: [
        NxLanguageDropdown,
        NxAccountSettingsDropdown,
        NxActiveSystemDropdown,
        NxSystemsDropdown
    ],
    entryComponents: [
        NxLanguageDropdown,
        NxAccountSettingsDropdown,
        NxActiveSystemDropdown,
        NxSystemsDropdown
    ],
    exports: [
        NxLanguageDropdown,
        NxAccountSettingsDropdown,
        NxActiveSystemDropdown,
        NxSystemsDropdown
    ]
})
export class DropdownsModule {
}

declare var angular: angular.IAngularStatic;
angular
    .module('cloudApp.directives')
    .directive('nxLanguageSelect', downgradeComponent({component: NxLanguageDropdown}) as angular.IDirectiveFactory)
    .directive('nxAccountSettingsSelect', downgradeComponent({component: NxAccountSettingsDropdown}) as angular.IDirectiveFactory)
    .directive('nxActiveSystem', downgradeComponent({component: NxActiveSystemDropdown}) as angular.IDirectiveFactory)
    .directive('nxSystems', downgradeComponent({component: NxSystemsDropdown}) as angular.IDirectiveFactory)
