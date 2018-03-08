import { TestBed } from '@angular/core/testing';
import { BarComponent } from './bar.component';

describe('App', () => {
    
  beforeEach(() => {
    TestBed.configureTestingModule({ 
      declarations: [BarComponent],
      providers: [{ provide: 'barService', useValue: { getData: () => {} } }]
    });
  });

  it ('should work', () => {
    let fixture = TestBed.createComponent(BarComponent);
    expect(fixture.componentInstance instanceof BarComponent).toBe(true, 'should create BarComponent');
  });
});