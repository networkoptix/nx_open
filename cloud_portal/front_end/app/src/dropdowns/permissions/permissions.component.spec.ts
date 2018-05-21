import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxPermissionsDropdown } from './permissions.component';

describe('NxPermissionsDropdown', () => {
    let component: NxPermissionsDropdown;
    let fixture: ComponentFixture<NxPermissionsDropdown>;

    beforeEach(async(() => {
        TestBed
                .configureTestingModule({
                    declarations: [NxPermissionsDropdown]
                })
                .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxPermissionsDropdown);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
