import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';
import { FormsModule }          from '@angular/forms';

import { NxContentComponent } from './content.component';

import { TranslateModule }  from '@ngx-translate/core';
import { ComponentsModule } from '../../components/components.module';

const appRoutes: Routes = [
    {
        path: 'new-content', component: NxContentComponent,
    }
];

// TODO: Remove it after test

@NgModule({
    imports        : [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        TranslateModule,
        ComponentsModule,
        FormsModule,

        RouterModule.forChild(appRoutes)
    ],
    providers      : [],
    declarations   : [
        NxContentComponent,
    ],
    bootstrap      : [],
    entryComponents: [
        NxContentComponent
    ],
    exports        : [
        NxContentComponent
    ]
})
export class ContentModule {
}
