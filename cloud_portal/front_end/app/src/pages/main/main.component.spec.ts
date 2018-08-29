import { TestBed }           from '@angular/core/testing';
import { NxMainComponent } from './main.component';

describe('App', () => {
    
  beforeEach(() => {
    TestBed.configureTestingModule({ 
      declarations: [NxMainComponent],
      providers: [{ provide: 'barService', useValue: { getData: () => {} } }]
    });
  });

  it ('should work', () => {
    let fixture = TestBed.createComponent(NxMainComponent);
    expect(fixture.componentInstance instanceof NxMainComponent).toBe(true, 'should create NxMainComponent');
  });
});

