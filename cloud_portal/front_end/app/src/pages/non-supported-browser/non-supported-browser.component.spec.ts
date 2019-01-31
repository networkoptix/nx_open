import { ComponentFixture, TestBed, async } from '@angular/core/testing';
import { NonSupportedBrowserComponent }     from './non-supported-browser.component';
import { NxCheckboxComponent }              from '../../components/checkbox/checkbox.component';

describe('NonSupportedBrowserComponent', () => {
    let component: NonSupportedBrowserComponent;
    let fixture: ComponentFixture<NonSupportedBrowserComponent>;

    beforeEach(async(() => {
        TestBed
                .configureTestingModule({
                    declarations: [NonSupportedBrowserComponent],
                    providers   : []
                })
                .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NonSupportedBrowserComponent);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create NonSupportedBrowserComponent', () => {
        fixture = TestBed.createComponent(NonSupportedBrowserComponent);
        component = fixture.componentInstance;
        expect(component).toBeTruthy();
    });
});
