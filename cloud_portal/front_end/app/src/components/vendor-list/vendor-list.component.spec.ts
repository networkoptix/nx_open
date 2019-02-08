import { async, ComponentFixture, inject, TestBed } from '@angular/core/testing';

import { NxVendorListComponent } from './vendor-list.component';
import { NxPreLoaderComponent }  from '../pre-loader/pre-loader.component';
import { NxRibbonService }       from '../ribbon/ribbon.service';

describe('NxVendorListComponent', () => {
    let component: NxVendorListComponent;
    let fixture: ComponentFixture<NxVendorListComponent>;

    beforeEach(async(() => {
        TestBed.configureTestingModule({
            declarations: [NxVendorListComponent, NxPreLoaderComponent]
        }).compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxVendorListComponent);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });

    it('renders correctly', () => {
        fixture.detectChanges();
        expect(fixture).toMatchSnapshot();
    });
});
