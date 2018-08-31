import { TestBed }           from '@angular/core/testing';
import { NxServersDetailComponent } from './servers.component';

describe('App', () => {
    
  beforeEach(() => {
    TestBed.configureTestingModule({ 
      declarations: [NxServersDetailComponent],
      providers: [{ provide: 'barService', useValue: { getData: () => {} } }]
    });
  });

  it ('should work', () => {
    let fixture = TestBed.createComponent(NxServersDetailComponent);
    expect(fixture.componentInstance instanceof NxServersDetailComponent).toBe(true, 'should create NxServersDetailComponent');
  });
});

