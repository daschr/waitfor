all:  comp perm
comp:
	bash -c "cd liboping && ./autogen.sh  && ./configure && make && sudo make install"
	gcc -O3 -o waitfor waitfor.c /usr/local/lib/liboping.a
perm:
	sudo setcap cap_net_raw+ep waitfor
clean:
	rm waitfor
test:
	./waitfor -t5 heise.de 80 && echo there || echo not there
	./waitfor heise.de && echo there || echo not there
	./waitfor -t2 heise.de && echo there || echo not there
	echo everything there?	
