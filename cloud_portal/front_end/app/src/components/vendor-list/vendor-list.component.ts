import {
    Component, EventEmitter, forwardRef, Input, OnInit, Output
}                                 from '@angular/core';
import { NxConfigService }        from '../../services/nx-config';
import { NG_VALUE_ACCESSOR }      from '@angular/forms';
import { ActivatedRoute, Router } from '@angular/router';

/* USAGE
 <nx-vendor-list
         vendors=[]
         cameras=[]
        [(ngModel)]="filterModel">
 </nx-vendor-list>
 */

@Component({
    selector   : 'nx-vendor-list',
    templateUrl: 'vendor-list.component.html',
    styleUrls  : ['vendor-list.component.scss'],
    providers: [{
        provide    : NG_VALUE_ACCESSOR,
        useExisting: forwardRef(() => NxVendorListComponent),
        multi      : true
    }]
})
export class NxVendorListComponent implements OnInit {
    @Input() vendors: any;
    @Input() cameras: any;

    CONFIG: any;
    arrGridColumns: number[];

    private filter: any = {};

    constructor(CONFIG: NxConfigService,
                private _router: Router,
                private _route: ActivatedRoute) {

        this.CONFIG = CONFIG.getConfig();

        this.arrGridColumns = Array.apply(undefined, Array(this.CONFIG.campage.vendorGroups)).map((x, i) => i);
    }

    ngOnInit() {}

    vendorGroup(n) {
        function nstart(m) {
            if (m === 0) {
                return 0;
            }

            return nstart(m - 1) + ncol(m - 1);
        }

        function ncol(m) {
            return m < (cols - (M * cols - total)) ? M : (M - 1);
        }

        const total = this.vendors.length,
              cols  = this.CONFIG.campage.vendorGroups,
              M     = Math.ceil(total / cols),
              n1    = nstart(n),
              n2    = ncol(n);

        return { start: n1, end: n1 + n2 };
    }

    // Form control functions
    // The method set in registerOnChange to emit changes back to the form
    private propagateChange = (_: any) => {};

    writeValue(value: any) {
        if (value) {
            this.filter = value;
        }
    }

    /**
     * Set the function to be called
     * when the control receives a change event.
     */
    registerOnChange(fn) {
        this.propagateChange = fn;
    }

    /**
     * Set the function to be called
     * when the control receives a touch event.
     */
    registerOnTouched(fn: () => void): void {
    }

    setVendor(value) {
        interface Params {
            [key: string]: any;
        }

        const queryParams: Params = {};

        this.filter.multiselects.find((select) => {
            if (select.id === 'vendors') {
                select.selected.push(value);

                queryParams[select.id] = select.selected;

                // changes the route without moving from the current view or
                // triggering a navigation event,
                this._router.navigate(['/campage'], {
                    queryParams,
                    relativeTo: this._route,
                    replaceUrl: true,
                    queryParamsHandling: 'merge',
                    // do not trigger navigation
                    // skipLocationChange : true
                });
            }
        });
        // Propagate component's value attribute (model)
        this.propagateChange({...this.filter});
    }

}
