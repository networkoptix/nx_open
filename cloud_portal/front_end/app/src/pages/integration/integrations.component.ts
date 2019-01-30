import { Component, OnInit }  from '@angular/core';
import { IntegrationService } from './integration.service';

@Component({
    selector   : 'integrations-component',
    templateUrl: 'integrations.component.html',
    styleUrls  : ['integrations.component.scss']
})

export class NxIntegrationsComponent implements OnInit {

    private allElements: any;
    private elements: any;
    private search: string;

    selectors = {
        access   : false,
        analytics: false,
        cameras  : false,
        home     : false,
        psim     : false,
    };

    private setupDefaults() {
        this.allElements = [];
        this.elements = [];
    }

    constructor(private integrations: IntegrationService) {
        this.setupDefaults();
    }

    ngOnInit(): void {
        this.integrations
            .pluginsSubject
            .subscribe((result: any) => {
                        if (result.length) {
                            this.allElements = result;
                            // array deep copy for filtering
                            this.elements = this.allElements.map(obj => ({ ...obj }));
                        }
                    },
                    error => {
                        console.error('Error -> ', error);
                    });
    }

    markMatch(item, text) {
        const pattern = new RegExp(text, 'gm');
        item.name = item.name.replace(pattern, '<span class="marked">' + text + '</span>');
    }

    searchFilterFor(text) {
        this.elements = this.allElements.map(obj => ({ ...obj }));
        this.elements = this.elements.filter(item => {
            if (item.name.indexOf(text) > -1 ||
                    item.description.indexOf(text) > -1) {

                this.markMatch(item, text);
                return item;
            }
        });
    }

    setFilter(value, target) {
        let showAll = true;
        this.selectors[target] = value; // model is not updated yet

        if (this.search === '') {
            this.elements = this.allElements.map(obj => ({ ...obj }));
        }

        this.elements = this.elements.filter(item => {
            if (this.selectors[item.type]) {
                showAll = false;
                return item;
            }
        });

        if (showAll) {

            this.elements = this.allElements.map(obj => ({ ...obj }));
        }
    }
}

