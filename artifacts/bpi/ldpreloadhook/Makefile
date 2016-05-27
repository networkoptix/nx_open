all:
	gcc -std=gnu99 -Wall -Wextra -Wwrite-strings -fPIC -c -o hook.o hook.c
	gcc -std=gnu99 -Wall -Wextra -Wwrite-strings -fPIC -c -o dev_fb.o dev_fb.c
	gcc -std=gnu99 -Wall -Wextra -Wwrite-strings -fPIC -c -o handle_fb_disp.o handle_fb_disp.c
	gcc -Wwrite-strings -shared -o ldpreloadhook.so hook.o dev_fb.o handle_fb_disp.o -lpthread -ldl

clean:
	rm -f *.so *.o
