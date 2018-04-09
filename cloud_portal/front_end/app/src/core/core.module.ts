import { NgModule, Optional, SkipSelf } from '@angular/core';
import { CommonModule } from '@angular/common';
import { RouteReuseStrategy, RouterModule } from '@angular/router';

export function createHttpService() {
}

@NgModule({
    imports: [
        CommonModule,
        // TranslateModule,
        // NgbModule,
        RouterModule
    ],
    declarations: [],
    providers: [
        // I18nService,
    ]
})
export class CoreModule {

    constructor(@Optional() @SkipSelf() parentModule: CoreModule) {
        // Import guard
        if (parentModule) {
            throw new Error(`${parentModule} has already been loaded. Import Core module in the AppModule only.`);
        }
    }

}
