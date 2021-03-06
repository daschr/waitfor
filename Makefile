CC=gcc



all:  oping comp
oping:
	bash -c "cd liboping; ./autogen.sh && ./configure && make"
comp:
	$(CC) -Iliboping/src/ -O3 -o waitfor waitfor.c liboping/src/.libs/liboping.a
perm:
	setcap cap_net_raw+ep waitfor
clean:
	rm -f waitfor
	bash -c "cd liboping; make clean"
	rm -f deb/waitfor.deb deb/waitfor/usr/bin/waitfor

package: waitfor
	cp waitfor deb/waitfor/usr/bin/
	bash -c "cd deb/ && dpkg -b waitfor waitfor.deb"
