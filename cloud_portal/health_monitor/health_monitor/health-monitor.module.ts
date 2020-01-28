import { NgModule } from '@angular/core';
import { BrowserModule, Title } from '@angular/platform-browser';
import { Location, HashLocationStrategy, LocationStrategy, CommonModule } from '@angular/common';
import { RouterModule } from '@angular/router';
import { HttpClientModule } from '@angular/common/http';
import { FormsModule } from '@angular/forms';
import { NgbModule } from '@ng-bootstrap/ng-bootstrap';
import { DeviceDetectorModule } from 'ngx-device-detector';

import { TranslateModule } from '@ngx-translate/core';
import { WebStorageModule } from 'ngx-store';

import { HealthMonitorComponent } from './health-monitor.component';

// Components


// Dialogs


// Directives


// Health page
import { NxHealthComponent } from '../app/src/pages/health/health.component';

// Route guards
import { AuthGuard } from '../app/src/routeGuards/authGuard';

// Services
import { WINDOWS_PROVIDERS } from '../app/src/services/window-provider';
import { NxHealthModule } from '../app/src/pages/health/health.module';
import { NxSystemAlertsComponent } from '../app/src/pages/health/alerts/alerts.component';
import { NxSystemMetricsComponent } from '../app/src/pages/health/metrics/metrics.component';

@NgModule({
    imports: [
        CommonModule,
        BrowserModule,
        FormsModule,
        NxHealthModule,
        HttpClientModule,
        NgbModule,
        WebStorageModule,
        DeviceDetectorModule.forRoot(),
        TranslateModule.forRoot(),
        RouterModule.forRoot([
            { path: 'health', component: NxHealthComponent,
                children: [
                    {
                        path: '', redirectTo: 'alerts',
                        pathMatch: 'full'
                    },
                    {
                        path: 'alerts', component: NxSystemAlertsComponent,
                    },
                    {
                        path: ':metric', component: NxSystemMetricsComponent,
                    }
                ]
            },
            { path: '**', redirectTo: 'health' }
        ], {
            useHash: true,
            initialNavigation: true,
            scrollPositionRestoration: 'enabled',
            anchorScrolling          : 'enabled',
            enableTracing            : false
        })
    ],
    entryComponents: [
        HealthMonitorComponent
    ],
    providers: [
        AuthGuard,
        Location,
        Title,
        WINDOWS_PROVIDERS,
        { provide: LocationStrategy, useClass: HashLocationStrategy },
    ],
    declarations: [
        HealthMonitorComponent
    ],
    bootstrap: [
        HealthMonitorComponent
    ]
})
export class HealthMonitorPageModule {
    ngDoBootstrap() {
    }
}
