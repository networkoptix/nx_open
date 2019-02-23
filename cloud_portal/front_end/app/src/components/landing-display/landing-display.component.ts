import { Component, OnInit } from '@angular/core';
import { NxConfigService }           from '../../services/nx-config';

@Component({
    selector   : 'landing-display-component',
    templateUrl: 'landing-display.component.html',
    styleUrls  : ['landing-display.component.scss']
})

export class NxLandingDisplayComponent implements OnInit {
    private CONFIG: any = {};
    params: any;
    userEmail: any;

    constructor(private config: NxConfigService) {
        this.CONFIG = config.getConfig();
    }

    ngOnInit(): void {
    }
}

