import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxActiveSystemDropdown } from './active-system.component';

describe('NxActiveSystemDropdown', () => {
    let component: NxActiveSystemDropdown;
    let fixture: ComponentFixture<NxActiveSystemDropdown>;

    beforeEach(async(() => {
        TestBed
                .configureTestingModule({
                    declarations: [NxActiveSystemDropdown]
                })
                .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxActiveSystemDropdown);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
