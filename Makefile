all:  oping comp
oping:
	git clone https://github.com/octo/liboping
	bash -c "cd liboping; ./autogen.sh && ./configure && make"
comp:
	gcc -Iliboping/src/ -O3 -o waitfor waitfor.c liboping/src/.libs/liboping.a
perm:
	sudo setcap cap_net_raw+ep waitfor
clean:
	rm -rf waitfor liboping
test:
	./waitfor -t5 heise.de 80 && echo there || echo not there
	./waitfor heise.de && echo there || echo not there
	./waitfor -t2 heise.de && echo there || echo not there
	echo everything there?	
