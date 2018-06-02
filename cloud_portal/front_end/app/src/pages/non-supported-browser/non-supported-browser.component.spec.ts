import { TestBed }                      from '@angular/core/testing';
import { NonSupportedBrowserComponent } from './non-supported-browser.component';

describe('App', () => {
    
  beforeEach(() => {
    TestBed.configureTestingModule({ 
      declarations: [NonSupportedBrowserComponent],
      providers: []
    });
  });

  it ('should work', () => {
    let fixture = TestBed.createComponent(NonSupportedBrowserComponent);
    expect(fixture.componentInstance instanceof NonSupportedBrowserComponent).toBe(true, 'should create NonSupportedBrowserComponent');
  });
});
