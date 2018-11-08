import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxLayoutComponent } from './layout.component';

describe('NxLayoutComponent', () => {
    let component: NxLayoutComponent;
    let fixture: ComponentFixture<NxLayoutComponent>;

    beforeEach(async(() => {
        TestBed.configureTestingModule({
                declarations: [NxLayoutComponent ]
            })
            .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxLayoutComponent);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
