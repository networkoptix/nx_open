import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { NxIntegrationDetailsComponent } from './details.component';

import { TranslateModule }      from '@ngx-translate/core';
import { ComponentsModule }     from '../../../components/components.module';
import { NxOverviewComponent }  from './overview/overview.component';
import { NxSetupComponent }     from './setup/setup.component';

const appRoutes: Routes = [
    {
        path    : 'integrations/:id', component: NxIntegrationDetailsComponent,
        children: [
            // { path: '', redirectTo: 'how-it-works', pathMatch: 'full' },
            // { path: 'how-it-works', component: NxOverviewComponent },
            { path: '', component: NxOverviewComponent },
            { path: 'how-to-install', component: NxSetupComponent },
        ]
    }
];

@NgModule({
    imports        : [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        RouterModule,
        NgbModule,
        TranslateModule,
        ComponentsModule,

        RouterModule.forChild(appRoutes)
    ],
    providers      : [],
    declarations   : [
        NxIntegrationDetailsComponent
    ],
    bootstrap      : [],
    entryComponents: [
        NxIntegrationDetailsComponent
    ],
    exports        : [
        NxIntegrationDetailsComponent
    ]
})
export class IntegrationDetailModule {
}
