import { Component, OnInit }  from '@angular/core';
import { IntegrationService } from './integration.service';
import { NxUriService }       from '../../services/uri.service';

@Component({
    selector   : 'integrations-component',
    templateUrl: 'integrations.component.html',
    styleUrls  : ['integrations.component.scss']
})

export class NxIntegrationsComponent implements OnInit {

    private allElements: any;
    private elements: any;
    private emptyFilter: any = {};
    private filterModel: any = {};
    params: any;

    selectors = {
        access   : false,
        analytics: false,
        cameras  : false,
        home     : false,
        psim     : false,
    };

    private setupDefaults() {
        this.allElements = [];

        this.emptyFilter = {
            query: ''
        };
        this.filterModel = this.emptyFilter;
        this.filterModel.tags = [];
    }

    constructor(private uri: NxUriService,
                private integrations: IntegrationService) {

        this.setupDefaults();
    }

    ngOnInit(): void {
        // Example URI
        // /integrations?search=node
        this.uri
            .getURI()
            .subscribe(params => {
                this.params = { ...params };
                this.filterModel.query = this.params.search || '';
            });

        this.integrations
            .pluginsSubject
            .subscribe((result: any) => {
                        if (result.length) {
                            this.allElements = result;
                            this.setTags();
                            this.setFilter();
                        }
                    },
                    error => {
                        console.error('Error -> ', error);
                    });
    }

    setTags() {

        this.allElements.forEach((integration) => {
            integration.information.type.forEach((type) => {
                const found = this.filterModel.tags.some((tag) => tag.id === type);
                if (!found) {
                    this.filterModel.tags.push({ id: type, label: type, value: false });
                }
            });
        });

        // Ecsure model change will be trigger
        this.filterModel = { ...this.filterModel };
    }

    setFilter() {
        this.elements = this.allElements.map(obj => ({ ...obj }));

        if (this.filterModel.query !== '') {
            const query = this.filterModel.query.toLowerCase();

            this.elements = this.elements.filter(item => {
                if (item.information['name'].toLowerCase().indexOf(query) > -1 ||
                        item.information['companyName'].toLowerCase().indexOf(query) > -1 ||
                        item.information['shortDescription'].toLowerCase().indexOf(query) > -1 ||
                        item.overview['description'].toLowerCase().indexOf(query) > -1) {

                    // this.markMatch(item, text);
                    return item;
                }
            });
        }

        if (this.filterModel.tags.length) {
            const hasTagSelection = this.filterModel.tags.some((tag) => tag.value);

            if (hasTagSelection) {
                this.elements = this.elements.filter(item => {
                    return item.information.type.find((type) => {
                        return this.filterModel.tags.some(tag => {
                            if (tag.label === type && tag.value) {
                                return item;
                            }
                        });
                    });
                });
            }
        }
    }

    modelChanged(searchModel): void {
        this.setFilter();
    }

    markMatch(item, text) {
        const pattern = new RegExp(text, 'gm');
        item.name = item.name.replace(pattern, '<span class="marked">' + text + '</span>');
    }
}

