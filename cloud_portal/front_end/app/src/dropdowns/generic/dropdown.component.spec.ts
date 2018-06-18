import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxGenericDropdown } from './dropdown.component';

describe('NxGenericDropdown', () => {
    let component: NxGenericDropdown;
    let fixture: ComponentFixture<NxGenericDropdown>;

    beforeEach(async(() => {
        TestBed
                .configureTestingModule({
                    declarations: [NxGenericDropdown]
                })
                .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxGenericDropdown);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
