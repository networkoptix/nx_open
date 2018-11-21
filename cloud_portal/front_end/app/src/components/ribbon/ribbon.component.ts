import { Component, Input, OnInit } from '@angular/core';
import { IntegrationService }       from '../../pages/integration/integration.service';

@Component({
    selector: 'nx-ribbon',
    templateUrl: 'ribbon.component.html',
    styleUrls: ['ribbon.component.scss'],
})
export class NxRibbonComponent implements OnInit {
    @Input() message: string;
    @Input() action: string;
    @Input() actionUrl: any;

    private inReview: boolean;

    constructor(private integrationService: IntegrationService) {
        this.inReview = false;
        this.integrationService.pluginsSubject.subscribe(plugins => {
            this.inReview = plugins.filter(plugin => {
                return plugin.pending;
            }).length > 0;
        });

        this.integrationService.selectedPluginSubject.subscribe(plugin => {
            if (plugin) {
                this.inReview = plugin.pending || false;
            }
        });
    }

    ngOnInit() {
    }
}
