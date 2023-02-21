Multi-threaded prime number generation, putting output into files.

First, compile and install primesieve:

	git clone --recursive https://github.com/kimwalisch/primesieve
	cd primesieve/
	cmake .
	make -j
	sudo make install
	sudo ldconfig

Then compile primegen:

	cc primegen.c -o primegen -O3 -lprimesieve

Run:

	./primegen
