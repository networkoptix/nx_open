import { NgModule }     from '@angular/core';

import { NxClickElsewhereDirective } from './nx-click-elsewhere';
import { NxFocusMeDirective }      from './nx-focus-me';

@NgModule({
    imports: [
    ],
    declarations: [
        NxClickElsewhereDirective,
        NxFocusMeDirective
    ],
    entryComponents: [
    ],
    exports: [
        NxClickElsewhereDirective,
        NxFocusMeDirective
    ]
})
export class DirectivesModule {
}
