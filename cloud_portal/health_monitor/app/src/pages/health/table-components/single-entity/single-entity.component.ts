import { Component, Input, OnChanges, ViewEncapsulation } from '@angular/core';
import { NxConfigService }                                from '../../../../services/nx-config';
import { NxUtilsService }                                 from '../../../../services/utils.service';
import { NxHealthService } from '../../health.service';

@Component({
    selector     : 'nx-single-entity',
    templateUrl  : './single-entity.component.html',
    styleUrls    : ['./single-entity.component.scss'],
    encapsulation: ViewEncapsulation.None
})
export class NxSingleEntityComponent implements OnChanges {
    @Input() params: any;
    @Input() entity: any;

    CONFIG: any = {};
    copyParams: any;
    entityName: string;

    constructor(private configService: NxConfigService,
                private healthService: NxHealthService,
                private utilsService: NxUtilsService) {
        this.CONFIG = this.configService.getConfig();
    }

    ngOnChanges(): void {
        this.copyParams = {...this.params};
        if (this.copyParams.values.length && this.copyParams.values[0].id === '_') {
            this.copyParams.values.shift();
        }
        this.entityName = this.healthService.findEntityName(this.entity);
    }
}
