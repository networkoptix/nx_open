import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxSystemsDropdown } from './systems.component';

describe('NxActiveSystemDropdown', () => {
    let component: NxSystemsDropdown;
    let fixture: ComponentFixture<NxSystemsDropdown>;

    beforeEach(async(() => {
        TestBed
                .configureTestingModule({
                    declarations: [NxSystemsDropdown]
                })
                .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxSystemsDropdown);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
