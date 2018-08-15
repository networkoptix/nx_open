import { NgModule }        from '@angular/core';
import { CommonModule }    from '@angular/common';
import { TranslateModule } from '@ngx-translate/core';

import { NxProcessButtonComponent } from './process-button/process-button.component';
import { NxPreLoaderComponent }     from './pre-loader/pre-loader.component';

@NgModule({
    imports: [
        CommonModule,
        TranslateModule
    ],
    declarations: [
        NxProcessButtonComponent,
        NxPreLoaderComponent
    ],
    entryComponents: [
        NxProcessButtonComponent,
        NxPreLoaderComponent
    ],
    providers: [
        NxProcessButtonComponent,
        NxPreLoaderComponent
    ],
    exports: [
        NxProcessButtonComponent,
        NxPreLoaderComponent
    ]
})
export class ComponentsModule {
}
