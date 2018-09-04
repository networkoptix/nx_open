import { NgModule }        from '@angular/core';
import { CommonModule }    from '@angular/common';
import { TranslateModule } from '@ngx-translate/core';
import { downgradeComponent }       from '@angular/upgrade/static';
import { RouterModule }             from '@angular/router';

import { NxProcessButtonComponent } from './process-button/process-button.component';
import { NxPreLoaderComponent }     from './pre-loader/pre-loader.component';
import { NxCheckboxComponent }      from './checkbox/checkbox.component';
import { NxRadioComponent }         from './radio/radio.component';
import { MenuModule }               from './menu/menu.module';

@NgModule({
    imports: [
        CommonModule,
        TranslateModule,
        RouterModule,
        MenuModule
    ],
    declarations: [
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent,
        NxRadioComponent
    ],
    entryComponents: [
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent,
        NxRadioComponent
    ],
    providers: [
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent,
        NxRadioComponent
    ],
    exports: [
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent,
        NxRadioComponent,
        MenuModule
    ]
})
export class ComponentsModule {
}

declare var angular: angular.IAngularStatic;
angular
    .module('cloudApp.directives')
    .directive('nxProcessButton', downgradeComponent({component: NxProcessButtonComponent}) as angular.IDirectiveFactory)
    .directive('nxPreLoader', downgradeComponent({component: NxPreLoaderComponent}) as angular.IDirectiveFactory);
