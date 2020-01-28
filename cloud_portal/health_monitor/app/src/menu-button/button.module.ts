import { NgModule }     from '@angular/core';
import { CommonModule } from '@angular/common';

import { ComponentsModule }      from '../components/components.module';
import { NxMenuButtonComponent } from './button.component';

@NgModule({
    imports        : [
        CommonModule,
    ],
    declarations   : [
        NxMenuButtonComponent,
    ],
    entryComponents: [

    ],
    providers      : [

    ],
    exports: [
        NxMenuButtonComponent
    ]
})
export class NxButtonModule {
}


