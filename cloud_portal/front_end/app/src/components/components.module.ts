import { NgModule }        from '@angular/core';
import { CommonModule }    from '@angular/common';
import { TranslateModule } from '@ngx-translate/core';

import { NxMenuComponent }          from './menu/menu.component';
import { NxProcessButtonComponent } from './process-button/process-button.component';
import { NxPreLoaderComponent }     from './pre-loader/pre-loader.component';
import { NxCheckboxComponent }      from './checkbox/checkbox.component';
import { NxRadioComponent }         from './radio/radio.component';
import { downgradeComponent }       from '@angular/upgrade/static';
import { RouterModule }             from '@angular/router';

@NgModule({
    imports: [
        CommonModule,
        TranslateModule,
        RouterModule
    ],
    declarations: [
        NxMenuComponent,
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent,
        NxRadioComponent
    ],
    entryComponents: [
        NxMenuComponent,
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent,
        NxRadioComponent
    ],
    providers: [
        NxMenuComponent,
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent,
        NxRadioComponent
    ],
    exports: [
        NxMenuComponent,
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent,
        NxRadioComponent
    ]
})
export class ComponentsModule {
}

declare var angular: angular.IAngularStatic;
angular
    .module('cloudApp.directives')
    .directive('nxProcessButton', downgradeComponent({component: NxProcessButtonComponent}) as angular.IDirectiveFactory)
    .directive('nxPreLoader', downgradeComponent({component: NxPreLoaderComponent}) as angular.IDirectiveFactory);
