import { NgModule }           from '@angular/core';
import { CommonModule }       from '@angular/common';
import { TranslateModule }    from '@ngx-translate/core';
import { downgradeComponent } from '@angular/upgrade/static';
import { RouterModule }       from '@angular/router';

import { NxProcessButtonComponent }       from './process-button/process-button.component';
import { NxPreLoaderComponent }           from './pre-loader/pre-loader.component';
import { NxCheckboxComponent }            from './checkbox/checkbox.component';
import { NxRadioComponent }               from './radio/radio.component';
import { MenuModule }                     from './menu/menu.module';
import { NxContentBlockComponent }        from './content-block/content-block.component';
import { NxContentBlockSectionComponent } from './content-block/section/section.component';
import { NxMultiLineEllipsisComponent }   from './multi-line-ellipsis/mle.component';
import { NxExternalVideoComponent }       from "./external-video/external-video.component";
import { NxLayoutComponent }              from './layout/layout.component';

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
        NxLayoutComponent,
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
        NxLayoutComponent,
    ],
    providers      : [
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent,
        NxRadioComponent,
        NxContentBlockComponent,
        NxContentBlockSectionComponent,
        NxMultiLineEllipsisComponent,
        NxLayoutComponent,
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
        NxLayoutComponent,
        MenuModule,
    ]
})
export class ComponentsModule {
}

declare var angular: angular.IAngularStatic;
angular
    .module('cloudApp.directives')
    .directive('nxProcessButton', downgradeComponent({ component: NxProcessButtonComponent }) as angular.IDirectiveFactory)
    .directive('nxPreLoader', downgradeComponent({ component: NxPreLoaderComponent }) as angular.IDirectiveFactory);
