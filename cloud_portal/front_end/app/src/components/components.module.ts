import { NgModule }        from '@angular/core';
import { CommonModule }    from '@angular/common';
import { TranslateModule } from '@ngx-translate/core';

import { NxProcessButtonComponent } from "./process-button/process-button.component";

@NgModule({
    imports: [
        CommonModule,
        TranslateModule
    ],
    declarations: [
        NxProcessButtonComponent
    ],
    entryComponents: [
        NxProcessButtonComponent
    ],
    providers: [
        NxProcessButtonComponent
    ],
    exports: [
        NxProcessButtonComponent
    ]
})
export class ComponentsModule {
}
