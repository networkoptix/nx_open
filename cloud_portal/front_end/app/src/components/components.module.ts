import { NgModule }        from '@angular/core';
import { CommonModule }    from '@angular/common';
import { TranslateModule } from '@ngx-translate/core';

import { NxProcessButtonComponent } from './process-button/process-button.component';
import { NxPreLoaderComponent }     from './pre-loader/pre-loader.component';
import { NxCheckboxComponent }      from './checkbox/checkbox.component';
import { downgradeComponent }       from '../../../node_modules/@angular/upgrade/static';
import { NxGenericDropdown }        from '../dropdowns/generic/dropdown.component';

@NgModule({
    imports: [
        CommonModule,
        TranslateModule
    ],
    declarations: [
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent
    ],
    entryComponents: [
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent
    ],
    providers: [
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent
    ],
    exports: [
        NxProcessButtonComponent,
        NxPreLoaderComponent,
        NxCheckboxComponent
    ]
})
export class ComponentsModule {
}

declare var angular: angular.IAngularStatic;
angular
    .module('cloudApp.directives')
    .directive('nxProcessButton', downgradeComponent({component: NxProcessButtonComponent}) as angular.IDirectiveFactory)
    .directive('nxPreLoader', downgradeComponent({component: NxPreLoaderComponent}) as angular.IDirectiveFactory)
    .directive('nxCheckbox', downgradeComponent({component: NxCheckboxComponent}) as angular.IDirectiveFactory);
