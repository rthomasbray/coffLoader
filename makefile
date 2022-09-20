coff:
	x86_64-w64-mingw32-gcc -Wall loader.c -o outdir/loader.exe
	x86_64-w64-mingw32-gcc -c testprogram.c -o outdir/testprogram.out
