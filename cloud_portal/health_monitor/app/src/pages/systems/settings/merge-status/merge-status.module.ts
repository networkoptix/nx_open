import { NgModule }                          from '@angular/core';
import { CommonModule }                      from '@angular/common';
import { BrowserModule }                     from '@angular/platform-browser';
import { TranslateModule }                   from '@ngx-translate/core';
import { ComponentsModule }                  from '../../../../components/components.module';
import { NxSystemMergeStatusComponent }      from './merge-status.component';

@NgModule({
    imports        : [
        CommonModule,
        BrowserModule,
        TranslateModule,
        ComponentsModule,
    ],
    providers      : [],
    declarations   : [
        NxSystemMergeStatusComponent
    ],
    bootstrap      : [],
    entryComponents: [
        NxSystemMergeStatusComponent
    ],
    exports        : [
        NxSystemMergeStatusComponent
    ]
})
export class NxSystemMergeStatusModule {
}
