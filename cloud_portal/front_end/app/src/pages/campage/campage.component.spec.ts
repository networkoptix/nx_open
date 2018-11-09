import { TestBed }           from '@angular/core/testing';
import { NxCampageComponent }  from './campage.component';

describe('App', () => {

  beforeEach(() => {
    TestBed.configureTestingModule({
      declarations: [NxCampageComponent],
      providers: [{ provide: 'barService', useValue: { getData: () => {} } }]
    });
  });

  it ('should work', () => {
    let fixture = TestBed.createComponent(NxCampageComponent);
    expect(fixture.componentInstance instanceof NxCampageComponent).toBe(true, 'should create NxCampageComponent');
  });
});