import { BrowserModule }    from '@angular/platform-browser';
import { NgModule }         from '@angular/core';
import { HttpClientModule } from '@angular/common/http';

import { AppComponent }      from './app.component';
import { InfoTileComponent } from './info-tile/info-tile.component';
import { TimeCheckedPipe }   from './pipes/time-checked.pipe';
import { TimeAgoPipe }       from './pipes/time-ago.pipe';
import { SlaPipe }           from './pipes/sla.pipe';

@NgModule({
    declarations: [
        AppComponent,
        InfoTileComponent,
        TimeCheckedPipe,
        TimeAgoPipe,
        SlaPipe
    ],
    imports: [
        BrowserModule,
        HttpClientModule
    ],
    providers: [],
    bootstrap: [AppComponent]
})
export class AppModule {
}
