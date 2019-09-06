import { Component, OnInit }  from '@angular/core';
import { Location }           from '@angular/common';
import { IntegrationService } from './integration.service';
import { NxUriService }       from '../../services/uri.service';
import { NxConfigService }    from '../../services/nx-config';
import { TranslateService }   from '@ngx-translate/core';
import { Title }              from '@angular/platform-browser';

@Component({
    selector   : 'integrations-component',
    templateUrl: 'integrations.component.html',
    styleUrls  : ['integrations.component.scss']
})

export class NxIntegrationsComponent implements OnInit {
    private CONFIG: any = {};
    private lang: any = {};
    private searchBy: any;
    private allElements: any;
    private elements: any;
    private emptyFilter: any = {};
    private filterModel: any = {};
    location: any;
    params: any;

    selectors = {
        access   : false,
        analytics: false,
        cameras  : false,
        home     : false,
        psim     : false,
    };

    private setupDefaults() {
        this.CONFIG = this.config.getConfig();

        this.allElements = [];
        this.elements = [];

        this.emptyFilter = {
            query: ''
        };
        this.filterModel = this.emptyFilter;
        this.filterModel.tags = [];
    }

    constructor(private uri: NxUriService,
                private integrations: IntegrationService,
                private config: NxConfigService,
                private translate: TranslateService,
                private title: Title,
                location: Location) {
        this.location = location;
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
                if (result) {
                    if (!this.CONFIG.integrationStoreEnabled && !result.length) {
                        this.location.go('404');
                    } else {
                        this.allElements = result;
                        this.setTags();
                        this.setFilter();
                    }
                } else {
                    this.elements = undefined;
                }
            },
            error => {
                console.error('Integration plugins error -> ', error);
                this.location.go('404');
            });

        setTimeout(() => {
            this.lang = this.translate.translations[this.translate.currentLang];
            this.title.setTitle(this.lang.pageTitles.integrations);
        });
    }

    setTags() {
        this.CONFIG.integrationFilterItems.forEach(item => {
            if (item.enabled) {
                    this.filterModel.tags.push({ id: item.name, label: item.name, value: false });
            }
        });

        // Ensure model change will be trigger
        this.filterModel = { ...this.filterModel };
    }

    setFilter() {
        function searchBy(item, query) {
            return (item.information.name && item.information.name.toLowerCase().indexOf(query) > -1 ||
                    item.information.companyName && item.information.companyName.toLowerCase().indexOf(query) > -1 ||
                    item.information.shortDescription && item.information.shortDescription.toLowerCase().indexOf(query) > -1 ||
                    item.overview.description && item.overview.description.toLowerCase().indexOf(query) > -1);
        }

        this.elements = this.allElements.map(obj => ({ ...obj }));

        if (this.filterModel.query !== '') {
            const query = this.filterModel.query.toLowerCase();

            this.elements = this.elements.filter(item => {
                if (searchBy(item, query)) {
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
        this.filterModel.query = searchModel.query;
        this.setFilter();
    }

    markMatch(item, text) {
        const pattern = new RegExp(text, 'gm');
        item.name = item.name.replace(pattern, '<span class="marked">' + text + '</span>');
    }
}

