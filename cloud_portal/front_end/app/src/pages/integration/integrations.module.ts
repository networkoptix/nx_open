import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';
import { FormsModule }          from '@angular/forms';

import { NxIntegrationsComponent } from './integrations.component';

import { TranslateModule }             from '@ngx-translate/core';
import { ComponentsModule }            from '../../components/components.module';
import { IntegrationsListModule }      from './list/list.module';
import { IntegrationDetailModule }     from './details/details.module';
import { NxIntegrationDetailsComponent } from './details/details.component';

const appRoutes: Routes = [
    { path    : 'integrations', component: NxIntegrationsComponent },
    { path    : 'integrations/:plugin', component: NxIntegrationDetailsComponent }
];

@NgModule({
    imports        : [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        TranslateModule,
        ComponentsModule,
        FormsModule,
        IntegrationsListModule,
        IntegrationDetailModule,

        RouterModule.forChild(appRoutes)
    ],
    providers      : [],
    declarations   : [
        NxIntegrationsComponent,
    ],
    bootstrap      : [],
    entryComponents: [
        NxIntegrationsComponent
    ],
    exports        : [
        NxIntegrationsComponent
    ]
})
export class IntegrationsModule {
}
