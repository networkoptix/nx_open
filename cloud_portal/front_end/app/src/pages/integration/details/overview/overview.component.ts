import { Component, OnInit, OnDestroy } from '@angular/core';
import { IntegrationService }           from '../../integration.service';

@Component({
    selector: 'overview-component',
    templateUrl: 'overview.component.html',
    styleUrls: ['overview.component.scss']
})

export class NxOverviewComponent implements OnInit, OnDestroy {

    plugin: any = {};

    private setupDefaults() {
        this.plugin = this.integrationService.getIntegrationPlugin();
        this.integrationService.setSection('how-it-works');
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

