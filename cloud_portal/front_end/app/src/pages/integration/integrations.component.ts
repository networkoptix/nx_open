import { Component, OnInit, OnChanges, SimpleChanges } from '@angular/core';
import { IntegrationService }                          from './integration.service';

@Component({
    selector   : 'integrations-component',
    templateUrl: 'integrations.component.html',
    styleUrls  : [ 'integrations.component.scss' ]
})

export class NxIntegrationsComponent implements OnInit, OnChanges {

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
            .getMockData()
            .subscribe((res: any) => {
                    this.allElements = res.integrations;
                    this.elements = this.allElements.map(obj => ({ ...obj })); // array deep copy
                },
                error => {
                    console.error('No mock data provided');
                });
    }

    markMatch(item, text) {
        const pattern = new RegExp(text, 'gm');
        item.name =  item.name.replace(pattern, '<span class="marked">' + text + '</span>');
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
        this.selectors[ target ] = value; // model is not updated yet

        if (this.search === '') {
            this.elements = this.allElements.map(obj => ({ ...obj }));
        }

        this.elements = this.elements.filter(item => {
            if (this.selectors[ item.type ]) {
                showAll = false;
                return item;
            }
        });

        if (showAll) {

            this.elements = this.allElements.map(obj => ({ ...obj }));
        }
    }

    ngOnChanges(change: SimpleChanges) {
        debugger;
        // this.elements = this.allElements.filter(item => {
        //
        //     let showAll = true;
        //     Object.keys(this.selectors).forEach(key => {
        //         if (this.selectors[ key ] && item.type === this.selectors[ key ]) {
        //             showAll = false;
        //             return item;
        //         }
        //     });
        // });
    }

}

