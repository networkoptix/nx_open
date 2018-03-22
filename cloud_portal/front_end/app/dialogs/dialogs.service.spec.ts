import { TestBed, inject } from '@angular/core/testing';

import { nxDialogsService } from './dialogs.service';

describe('nxDialogsService', () => {
  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [nxDialogsService]
    });
  });

  it('should be created', inject([nxDialogsService], (service: nxDialogsService) => {
    expect(service).toBeTruthy();
  }));
});
