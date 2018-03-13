import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxLanguageDropdown } from './language.component';

describe('NxLanguageDropdown', () => {
    let component: NxLanguageDropdown;
    let fixture: ComponentFixture<NxLanguageDropdown>;

    beforeEach(async(() => {
        TestBed
                .configureTestingModule({
                    declarations: [NxLanguageDropdown]
                })
                .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxLanguageDropdown);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
