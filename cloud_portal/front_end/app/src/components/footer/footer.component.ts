import { Component, OnInit } from '@angular/core';
import { DomSanitizer }      from '@angular/platform-browser';
import { NxConfigService }   from '../../services/nx-config';
import { NxAppStateService } from '../../services/nx-app-state.service';

@Component({
    selector: 'nx-footer',
    templateUrl: 'footer.component.html',
    styleUrls: [ 'footer.component.scss' ]
})
 export class NxFooterComponent implements OnInit {
    config: any;
    footerItems: any;
    viewFooter: boolean;

    constructor(private sanitizer: DomSanitizer,
                private _config: NxConfigService,
                private appState: NxAppStateService) {
        this.config = this._config.getConfig();
    }

    ngOnInit() {
        this.footerItems = this.config.footerItems;
        this.appState.footerVisibleObservable.subscribe((visible) => {
            this.viewFooter = visible;
        });
    }
}