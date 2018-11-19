import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxLayoutRightComponent } from './layout.component';

describe('NxLayoutRightComponent', () => {
    let component: NxLayoutRightComponent;
    let fixture: ComponentFixture<NxLayoutRightComponent>;

    beforeEach(async(() => {
        TestBed.configureTestingModule({
                declarations: [NxLayoutRightComponent ]
            })
            .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxLayoutRightComponent);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
