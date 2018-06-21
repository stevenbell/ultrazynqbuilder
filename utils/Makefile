
CROSS_COMPILE=aarch64-linux-gnu-

peek: peek.cpp
	$(CROSS_COMPILE)g++ -g peek.cpp -o peek

poke: poke.cpp
	$(CROSS_COMPILE)g++ -g poke.cpp -o poke

testcma: testcma.cpp
	$(CROSS_COMPILE)g++ -g -I ../drivers/ testcma.cpp -o testcma

babysit_cmabuf: babysit_cmabuf.cpp
	$(CROSS_COMPILE)g++ -I cma_test babysit_cmabuff.cpp cma_test/cma.c -o babysit_cmabuf

clean:
	rm twoinput testcma grabimage grabprocess testhw.cpp testhw

