import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxContentBlockHeaderComponent } from './header.component';

describe('NxContentBlockHeaderComponent', () => {
    let component: NxContentBlockHeaderComponent;
    let fixture: ComponentFixture<NxContentBlockHeaderComponent>;

    beforeEach(async(() => {
        TestBed.configureTestingModule({
                declarations: [ NxContentBlockHeaderComponent ]
            })
            .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxContentBlockHeaderComponent);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
