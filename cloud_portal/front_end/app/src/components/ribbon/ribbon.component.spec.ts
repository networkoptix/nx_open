import {
  async,
  ComponentFixture,
  TestBed,
  inject
} from '@angular/core/testing';
import { NxRibbonComponent } from './ribbon.component';
import { NxRibbonService } from './ribbon.service';

describe('NxRibbonComponent', () => {
  let component: NxRibbonComponent;
  let fixture: ComponentFixture<NxRibbonComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxRibbonComponent],
      providers: [NxRibbonService]
    }).compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxRibbonComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create NxRibbonComponent', () => {
    expect(component).toBeTruthy();
  });

  it('should be initialized', () => {
    component.ngOnInit();
    expect(component.showRibbon).toBeFalsy();
    expect(component.message).toBe('');
    expect(component.action).toBe('');
    expect(component.actionUrl).toBe('');
  });

  it('should use NxRibbonService to get data', inject(
    [NxRibbonService],
    (service: NxRibbonService) => {
      const context = {
        message:
          'Alcohol! Because no great story started with someone eating a salad.',
        text: 'Go back',
        url: '/admin/cms/product/'
      };

      service.show(context.message, context.text, context.url);

      expect(component.showRibbon).toBeTruthy();
      expect(component.message).toBe(context.message);
      expect(component.action).toBe(context.text);
      expect(component.actionUrl).toBe(context.url);
    }
  ));

  it('should use NxRibbonService to hide and reset data', inject(
    [NxRibbonService],
    (service: NxRibbonService) => {
      service.hide();

      expect(component.showRibbon).toBeFalsy();
      expect(component.message).toBe('');
      expect(component.action).toBe('');
      expect(component.actionUrl).toBe('');
    }
  ));

  it('renders correctly', inject(
    [NxRibbonService],
    (service: NxRibbonService) => {
      const context = {
        message:
          'Alcohol! Because no great story started with someone eating a salad.',
        text: 'Go back',
        url: '/admin/cms/product/'
      };

      service.show(context.message, context.text, context.url);
      fixture.detectChanges();
      expect(fixture).toMatchSnapshot();
    }
  ));
});
