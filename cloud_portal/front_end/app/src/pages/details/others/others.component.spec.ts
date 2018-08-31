import { TestBed }           from '@angular/core/testing';
import { NxOtherDetailsComponent } from './others.component';

describe('NxOtherDetailsComponent', () => {
    
  beforeEach(() => {
    TestBed.configureTestingModule({ 
      declarations: [NxOtherDetailsComponent],
      providers: [{ provide: 'barService', useValue: { getData: () => {} } }]
    });
  });

  it ('should work', () => {
    let fixture = TestBed.createComponent(NxOtherDetailsComponent);
    expect(fixture.componentInstance instanceof NxOtherDetailsComponent).toBe(true, 'should create NxOtherDetailsComponent');
  });
});

