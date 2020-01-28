import { NgModule }                          from '@angular/core';
import { CommonModule }                      from '@angular/common';
import { BrowserModule }                     from '@angular/platform-browser';
import { downgradeComponent, UpgradeModule } from '@angular/upgrade/static';
import { AngularSvgIconModule }              from 'angular-svg-icon';
import { RouterModule, Routes }              from '@angular/router';
import { FormsModule }                       from '@angular/forms';
import { NgbModule }                         from '@ng-bootstrap/ng-bootstrap';

import { DirectivesModule }       from '../../../../directives/directives.module';
import { NxSystemAdminComponent } from './admin.component';

import { TranslateModule }     from '@ngx-translate/core';
import { ComponentsModule }    from '../../../../components/components.module';

@NgModule({
    imports        : [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        RouterModule,
        FormsModule,
        NgbModule,
        TranslateModule,
        ComponentsModule,
        DirectivesModule,
        AngularSvgIconModule,
    ],
    providers      : [],
    declarations   : [
        NxSystemAdminComponent
    ],
    bootstrap      : [],
    entryComponents: [
        NxSystemAdminComponent
    ],
    exports        : [
        NxSystemAdminComponent
    ]
})
export class NxSystemAdminModule {
}
