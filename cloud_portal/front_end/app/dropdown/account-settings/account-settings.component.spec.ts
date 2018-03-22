import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxAccountSettingsDropdown } from './account-settings.component';

describe('NxAccountSettingsDropdown', () => {
    let component: NxAccountSettingsDropdown;
    let fixture: ComponentFixture<NxAccountSettingsDropdown>;

    beforeEach(async(() => {
        TestBed
                .configureTestingModule({
                    declarations: [NxAccountSettingsDropdown]
                })
                .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxAccountSettingsDropdown);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
