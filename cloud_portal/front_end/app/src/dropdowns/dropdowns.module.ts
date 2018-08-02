import { NgModule }     from '@angular/core';
import { CommonModule } from '@angular/common';

import { NxGenericDropdown }                 from './generic/dropdown.component';
import { NxAccountSettingsDropdown }         from './account-settings/account-settings.component';
import { NxLanguageDropdown }                from './language/language.component';
import { NxActiveSystemDropdown }            from './active-system/active-system.component';
import { NxSystemsDropdown }                 from './systems/systems.component';
import { NxPermissionsDropdown }             from './permissions/permissions.component';
import { downgradeComponent }                from '@angular/upgrade/static';

import { DirectivesModule }                  from '../directives/directives.module';

@NgModule({
    imports: [
        CommonModule,
        DirectivesModule,
    ],
    declarations: [
        NxGenericDropdown,
        NxLanguageDropdown,
        NxAccountSettingsDropdown,
        NxActiveSystemDropdown,
        NxSystemsDropdown,
        NxPermissionsDropdown
    ],
    entryComponents: [
        NxGenericDropdown,
        NxLanguageDropdown,
        NxAccountSettingsDropdown,
        NxActiveSystemDropdown,
        NxSystemsDropdown,
        NxPermissionsDropdown
    ],
    exports: [
        NxGenericDropdown,
        NxLanguageDropdown,
        NxAccountSettingsDropdown,
        NxActiveSystemDropdown,
        NxSystemsDropdown,
        NxPermissionsDropdown
    ]
})
export class DropdownsModule {
}

declare var angular: angular.IAngularStatic;
angular
    .module('cloudApp.directives')
    .directive('nxSelect', downgradeComponent({component: NxGenericDropdown}) as angular.IDirectiveFactory)
    .directive('nxLanguageSelect', downgradeComponent({component: NxLanguageDropdown}) as angular.IDirectiveFactory)
    .directive('nxAccountSettingsSelect', downgradeComponent({component: NxAccountSettingsDropdown}) as angular.IDirectiveFactory)
    .directive('nxActiveSystem', downgradeComponent({component: NxActiveSystemDropdown}) as angular.IDirectiveFactory)
    .directive('nxSystems', downgradeComponent({component: NxSystemsDropdown}) as angular.IDirectiveFactory)
    .directive('nxPermissions', downgradeComponent({component: NxPermissionsDropdown}) as angular.IDirectiveFactory);
