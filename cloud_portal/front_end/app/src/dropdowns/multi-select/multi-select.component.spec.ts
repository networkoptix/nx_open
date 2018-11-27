import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxMultiSelectDropdown } from './multi-select.component';

describe('NxMultiSelectDropdown', () => {
    let component: NxMultiSelectDropdown;
    let fixture: ComponentFixture<NxMultiSelectDropdown>;

    beforeEach(async(() => {
        TestBed
                .configureTestingModule({
                    declarations: [NxMultiSelectDropdown]
                })
                .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxMultiSelectDropdown);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
