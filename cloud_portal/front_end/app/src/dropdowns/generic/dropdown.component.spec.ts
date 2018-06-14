import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxDropdown } from './dropdown.component';

describe('NxDropdown', () => {
    let component: NxDropdown;
    let fixture: ComponentFixture<NxDropdown>;

    beforeEach(async(() => {
        TestBed
                .configureTestingModule({
                    declarations: [NxDropdown]
                })
                .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxDropdown);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
