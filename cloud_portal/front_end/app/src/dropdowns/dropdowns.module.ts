import { NgModule }     from '@angular/core';
import { CommonModule } from '@angular/common';

import { NxGenericDropdown }         from './generic/dropdown.component';
import { NxAccountSettingsDropdown } from './account-settings/account-settings.component';
import { NxLanguageDropdown }        from './language/language.component';
import { NxActiveSystemDropdown }    from './active-system/active-system.component';
import { NxSystemsDropdown }         from './systems/systems.component';
import { NxPermissionsDropdown }     from './permissions/permissions.component';
import { NxMultiSelectDropdown }     from './multi-select/multi-select.component';
import { downgradeComponent }        from '@angular/upgrade/static';

import { DirectivesModule } from '../directives/directives.module';
import { TranslateModule }  from '@ngx-translate/core';
import { ComponentsModule } from '../components/components.module';
import { FormsModule }      from '@angular/forms';

@NgModule({
    imports        : [
        CommonModule,
        DirectivesModule,
        TranslateModule,
        ComponentsModule,
        FormsModule,
    ],
    declarations   : [
        NxGenericDropdown,
        NxLanguageDropdown,
        NxAccountSettingsDropdown,
        NxActiveSystemDropdown,
        NxSystemsDropdown,
        NxPermissionsDropdown,
        NxMultiSelectDropdown,
    ],
    entryComponents: [
        NxGenericDropdown,
        NxLanguageDropdown,
        NxAccountSettingsDropdown,
        NxActiveSystemDropdown,
        NxSystemsDropdown,
        NxPermissionsDropdown,
        NxMultiSelectDropdown,
    ],
    exports        : [
        NxGenericDropdown,
        NxLanguageDropdown,
        NxAccountSettingsDropdown,
        NxActiveSystemDropdown,
        NxSystemsDropdown,
        NxPermissionsDropdown,
        NxMultiSelectDropdown,
    ]
})
export class DropdownsModule {
}

declare var angular: angular.IAngularStatic;
angular
    .module('cloudApp.directives')
    .directive('nxLanguageSelect', downgradeComponent({component: NxLanguageDropdown}) as angular.IDirectiveFactory)
    .directive('nxSelect', downgradeComponent({ component: NxGenericDropdown }) as angular.IDirectiveFactory)
    .directive('nxMultiSelect', downgradeComponent({ component: NxMultiSelectDropdown }) as angular.IDirectiveFactory)
    .directive('nxAccountSettingsSelect', downgradeComponent({ component: NxAccountSettingsDropdown }) as angular.IDirectiveFactory)
    .directive('nxActiveSystem', downgradeComponent({ component: NxActiveSystemDropdown }) as angular.IDirectiveFactory)
    .directive('nxSystems', downgradeComponent({ component: NxSystemsDropdown }) as angular.IDirectiveFactory)
    .directive('nxPermissions', downgradeComponent({ component: NxPermissionsDropdown }) as angular.IDirectiveFactory);
