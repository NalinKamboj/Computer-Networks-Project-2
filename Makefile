USERID=TrollPirate

all:
	g++ -std=c++0x testfile.cpp -o router -pthread

clean:
	rm -rf *.o *~ *.gch *.swp *.dSYM all *.tar.gz

tarball: clean
	tar -cvf $(USERID).tar.gz *