import { NgModule }           from '@angular/core';
import { CommonModule }       from '@angular/common';
import { TranslateModule }    from '@ngx-translate/core';
import { downgradeComponent, downgradeInjectable } from '@angular/upgrade/static';
import { RouterModule }       from '@angular/router';

import { NxProcessButtonComponent }       from './process-button/process-button.component';
import { NxPreLoaderComponent }           from './pre-loader/pre-loader.component';
import { NxCheckboxComponent }            from './checkbox/checkbox.component';
import { NxRadioComponent }               from './radio/radio.component';
import { MenuModule }                     from './menu/menu.module';
import { NxContentBlockComponent }        from './content-block/content-block.component';
import { NxContentBlockSectionComponent } from './content-block/section/section.component';
import { NxMultiLineEllipsisComponent }   from './multi-line-ellipsis/mle.component';
import { NxExternalVideoComponent }       from './external-video/external-video.component';
import { NxLayoutRightComponent }         from './layout-right/layout.component';
import { NxTagComponent }                 from './tag/tag.component';
import { NxCarouselComponent }            from './carousel/carousel.component';
import { NxRibbonComponent }              from './ribbon/ribbon.component';
import { NxRibbonService }                from './ribbon/ribbon.service';
import { NxVendorListComponent }          from './vendor-list/vendor-list.component';

@NgModule({
    imports        : [
        CommonModule,
        TranslateModule,
        RouterModule,
        MenuModule
    ],
    declarations   : [
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
    ],
    entryComponents: [
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

        NxRibbonService,
    ],
    exports        : [
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
        MenuModule,
    ]
})
export class ComponentsModule {
}

declare var angular: angular.IAngularStatic;
angular
        .module('cloudApp.directives')
        .directive('nxProcessButton', downgradeComponent({ component: NxProcessButtonComponent }) as angular.IDirectiveFactory)
        .directive('nxPreLoader', downgradeComponent({ component: NxPreLoaderComponent }) as angular.IDirectiveFactory)
        .directive('nxRibbon', downgradeComponent({ component: NxRibbonComponent }) as angular.IDirectiveFactory);

angular
        .module('cloudApp.services')
        .service('NxRibbonService', downgradeInjectable(NxRibbonService));
