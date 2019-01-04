import { TestBed }           from '@angular/core/testing';
import { NxCampageComponent }  from './campage.component';

describe('App', () => {

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [NxCampageComponent],
      providers: [{ provide: 'barService', useValue: { getData: () => {} } }]
    });
  });

  it ('should create NxCampageComponent', () => {
    const fixture = TestBed.createComponent(NxCampageComponent);
    expect(fixture.componentInstance instanceof NxCampageComponent).toBeTruthy();
  });
});
