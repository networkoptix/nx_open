import { NgModule }                                from '@angular/core';
import { CommonModule }                            from '@angular/common';
import { TranslateModule }                         from '@ngx-translate/core';
import { downgradeComponent, downgradeInjectable } from '@angular/upgrade/static';
import { RouterModule }                            from '@angular/router';
import { FormsModule }                             from '@angular/forms';

import { DirectivesModule }               from '../directives/directives.module';
import { NxProcessButtonComponent }       from './process-button/process-button.component';
import { NxPreLoaderComponent }           from './pre-loader/pre-loader.component';
import { NxCheckboxComponent }            from './checkbox/checkbox.component';
import { NxRadioComponent }               from './radio/radio.component';
import { MenuModule }                     from './menu/menu.module';
import { NxContentBlockComponent }        from './content-block/content-block.component';
import { NxContentBlockSectionComponent } from './content-block/section/section.component';
import { NxMultiLineEllipsisComponent }   from './multi-line-ellipsis/mle.component';
import { NxExternalVideoComponent }       from './external-video/external-video.component';
import { NxLayoutRightComponent }    from './layout-right/layout.component';
import { NxTagComponent }            from './tag/tag.component';
import { NxCarouselComponent }       from './carousel/carousel.component';
import { NxRibbonComponent }         from './ribbon/ribbon.component';
import { NxRibbonService }           from './ribbon/ribbon.service';
import { NxVendorListComponent }     from './vendor-list/vendor-list.component';
import { NxSearchComponent }         from './search/search.component';
import { NxGenericDropdown }         from './dropdowns/generic/dropdown.component';
import { NxLanguageDropdown }        from './dropdowns/language/language.component';
import { NxAccountSettingsDropdown } from './dropdowns/account-settings/account-settings.component';
import { NxActiveSystemDropdown }    from './dropdowns/active-system/active-system.component';
import { NxSystemsDropdown }         from './dropdowns/systems/systems.component';
import { NxPermissionsDropdown }     from './dropdowns/permissions/permissions.component';
import { NxMultiSelectDropdown }     from './dropdowns/multi-select/multi-select.component';

@NgModule({
    imports        : [
        CommonModule,
        DirectivesModule,
        TranslateModule,
        RouterModule,
        FormsModule,
        MenuModule
    ],
    declarations   : [
        NxGenericDropdown,
        NxLanguageDropdown,
        NxAccountSettingsDropdown,
        NxActiveSystemDropdown,
        NxSystemsDropdown,
        NxPermissionsDropdown,
        NxMultiSelectDropdown,
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent,
        NxRadioComponent,
        NxContentBlockComponent,
        NxContentBlockSectionComponent,
        NxMultiLineEllipsisComponent,
        NxExternalVideoComponent,
        NxLayoutRightComponent,
        NxTagComponent,
        NxCarouselComponent,
        NxRibbonComponent,
        NxVendorListComponent,
        NxSearchComponent,
    ],
    entryComponents: [
        NxGenericDropdown,
        NxLanguageDropdown,
        NxAccountSettingsDropdown,
        NxActiveSystemDropdown,
        NxSystemsDropdown,
        NxPermissionsDropdown,
        NxMultiSelectDropdown,
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent,
        NxRadioComponent,
        NxContentBlockComponent,
        NxContentBlockSectionComponent,
        NxMultiLineEllipsisComponent,
        NxExternalVideoComponent,
        NxLayoutRightComponent,
        NxTagComponent,
        NxCarouselComponent,
        NxRibbonComponent,
        NxVendorListComponent,
        NxSearchComponent,
    ],
    providers      : [
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent,
        NxRadioComponent,
        NxContentBlockComponent,
        NxContentBlockSectionComponent,
        NxMultiLineEllipsisComponent,
        NxLayoutRightComponent,
        NxTagComponent,
        NxCarouselComponent,
        NxRibbonComponent,
        NxVendorListComponent,
        NxSearchComponent,

        NxRibbonService,
    ],
    exports        : [
        NxGenericDropdown,
        NxLanguageDropdown,
        NxAccountSettingsDropdown,
        NxActiveSystemDropdown,
        NxSystemsDropdown,
        NxPermissionsDropdown,
        NxMultiSelectDropdown,
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent,
        NxRadioComponent,
        NxContentBlockComponent,
        NxContentBlockSectionComponent,
        NxMultiLineEllipsisComponent,
        NxExternalVideoComponent,
        NxLayoutRightComponent,
        NxTagComponent,
        NxCarouselComponent,
        NxRibbonComponent,
        NxVendorListComponent,
        NxSearchComponent,
        MenuModule,
    ]
})
export class ComponentsModule {
}

declare var angular: angular.IAngularStatic;
angular
        .module('cloudApp.directives')
        .directive('nxLanguageSelect', downgradeComponent({ component: NxLanguageDropdown }) as angular.IDirectiveFactory)
        .directive('nxSelect', downgradeComponent({ component: NxGenericDropdown }) as angular.IDirectiveFactory)
        .directive('nxMultiSelect', downgradeComponent({ component: NxMultiSelectDropdown }) as angular.IDirectiveFactory)
        .directive('nxAccountSettingsSelect', downgradeComponent({ component: NxAccountSettingsDropdown }) as angular.IDirectiveFactory)
        .directive('nxActiveSystem', downgradeComponent({ component: NxActiveSystemDropdown }) as angular.IDirectiveFactory)
        .directive('nxSystems', downgradeComponent({ component: NxSystemsDropdown }) as angular.IDirectiveFactory)
        .directive('nxPermissions', downgradeComponent({ component: NxPermissionsDropdown }) as angular.IDirectiveFactory)
        .directive('nxProcessButton', downgradeComponent({ component: NxProcessButtonComponent }) as angular.IDirectiveFactory)
        .directive('nxPreLoader', downgradeComponent({ component: NxPreLoaderComponent }) as angular.IDirectiveFactory)
        .directive('nxRibbon', downgradeComponent({ component: NxRibbonComponent }) as angular.IDirectiveFactory);

angular
        .module('cloudApp.services')
        .service('NxRibbonService', downgradeInjectable(NxRibbonService));
