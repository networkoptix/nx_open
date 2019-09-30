import { Component, OnInit, OnDestroy } from '@angular/core';
import { IntegrationService }           from '../../integration.service';

@Component({
    selector: 'setup-component',
    templateUrl: 'setup.component.html',
    styleUrls: ['setup.component.scss']
})

export class NxSetupComponent implements OnInit, OnDestroy {

    plugin: any = {};

    private setupDefaults() {
        this.plugin = this.integrationService.getIntegrationPlugin();
        this.integrationService.setSection('how-to-install');
    }

    constructor(private integrationService: IntegrationService) {
        this.setupDefaults();
    }

    ngOnInit(): void {
    }

    ngOnDestroy() {
    }

    onSubmit() {
    }
}

