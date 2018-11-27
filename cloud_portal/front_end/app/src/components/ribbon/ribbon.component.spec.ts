import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxRibbonComponent } from './ribbon.component';

describe('NxRibbonComponent', () => {
    let component: NxRibbonComponent;
    let fixture: ComponentFixture<NxRibbonComponent>;

    beforeEach(async(() => {
        TestBed.configureTestingModule({
            declarations: [NxRibbonComponent]
        })
                .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxRibbonComponent);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
