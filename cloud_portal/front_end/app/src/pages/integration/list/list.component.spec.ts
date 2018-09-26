import { TestBed }           from '@angular/core/testing';
import { NxUsersDetailComponent } from './users.component';

describe('NxUsersDetailComponent', () => {
    
  beforeEach(() => {
    TestBed.configureTestingModule({ 
      declarations: [NxUsersDetailComponent],
      providers: [{ provide: 'barService', useValue: { getData: () => {} } }]
    });
  });

  it ('should work', () => {
    let fixture = TestBed.createComponent(NxUsersDetailComponent);
    expect(fixture.componentInstance instanceof NxUsersDetailComponent).toBe(true, 'should create NxUsersDetailComponent');
  });
});

