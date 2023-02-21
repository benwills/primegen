#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdbool.h>
#include <unistd.h>

#include <x86intrin.h> // for __rdtsc()
#include <primesieve.h>


// @CREDITS: hard-coded primes for random number generation come from xxhash
//           https://github.com/Cyan4973/xxHash/
// @CREDITS: uses primesieve for prime generation
//           https://github.com/kimwalisch/primesieve/


#define DBG_FFL {printf("DBG: File:[%s] Func:[%s] Line:[%d]\n",\
                 __FILE__, __FUNCTION__, __LINE__);fflush(stdout);}


//------------------------------------------------------------------------------
typedef struct
thrInfo_st {
  pthread_t thrId;
  int       thrNum;
} thrInfo_st;

typedef struct primegen_cfg_st {
	char*    dirOut;
	uint64_t primeMin;
	uint64_t primeMax;
	int      thrCnt;
	int      thrPrimeLoops;
	int      thrPrimesPerLoop;
} primegen_cfg_st;

primegen_cfg_st cfg = {
  .dirOut           = "out.primes.64",
  .primeMin         = UINT32_MAX,
  .primeMax         = UINT64_MAX,
  .thrCnt           = 2,
  .thrPrimeLoops    = 4,
  .thrPrimesPerLoop = 4096,
};

//------------------------------------------------------------------------------
// i believe this is a merseinne twister i found at some point
static uint64_t rand_x = 0;
static uint64_t rand_y = 0;

void
rand_init() {
  rand_x = __rdtsc() * 0x9E3779B185EBCA87ULL;
  rand_y = __rdtsc() * 0xC2B2AE3D27D4EB4FULL;
}

uint64_t
rand_get(void) {
  rand_x  = rand_y;
  rand_x ^= (rand_x << 23);
  rand_x ^= (rand_x >> 17);
  rand_x ^= rand_y ^ (rand_y >> 26);
  rand_y  = rand_x;
  return rand_x + rand_y;
}

//------------------------------------------------------------------------------
// get a new starting prime range, based on the thread number.
// this helps to ensure more of a range of primes.
// it's likely unnecessary, as rang_get() should handle that just fine.
// but might as well try.
uint64_t
primesNewMin(int thrId)
{
	const uint64_t rangeCnt = (cfg.primeMax - cfg.primeMin) / cfg.thrCnt;
	const uint64_t rangeMin = cfg.primeMin + (thrId * rangeCnt);
	const uint64_t rangeMax = rangeMin + rangeCnt;

	uint64_t p = 0;
  do {
  	p = rand_get();
  	// print to check how many times we're having to run this...
  	// averages slightly more than nThreads/2 times, which makes sense.
  	// for larger numbers of threads, it may make sense to pre-calculate these.
  	// printf(".");fflush(stdout);
  } while (p <= rangeMin || p >= rangeMax);

  return p;
}

//------------------------------------------------------------------------------
static void *
thrRun(void *arg)
{
  thrInfo_st *thrInfo = arg;
  printf("thrNum [%d] starting...\n", thrInfo->thrNum);
  fflush(stdout);

  for (int loop = 0; loop < cfg.thrPrimeLoops; loop++)
  {
	  const uint64_t primeMin = primesNewMin(thrInfo->thrNum);

	  // not sure of the performance cost of reallocing on every batch...
	  // previously used a different method with a single alloc per thread...
	  // but this was recommended in the documentation, so trying that...
	  uint64_t* primesFound
	  	= (uint64_t*)primesieve_generate_n_primes(cfg.thrPrimesPerLoop,
	  	                                          primeMin,
	  	                                          UINT64_PRIMES);

	  char fName[256] = {0};
	  sprintf(fName, "./%s/%"PRIu64".primes.u64.bin", cfg.dirOut, primesFound[0]);
	  FILE *fp = NULL;
	  if (NULL == (fp = fopen(fName, "wb"))) {
	  	printf("can't open file for output: %s\n", fName);
	  	exit(1);
	  }
	  fwrite(primesFound, sizeof(*primesFound), cfg.thrPrimesPerLoop, fp);
	  fflush(fp);

	  primesieve_free(primesFound);

	  printf("thr: %3d  completed loop: %4d of %4d\n",
	         thrInfo->thrNum, loop+1, cfg.thrPrimeLoops);
	  fflush(stdout);
  }

  return (int*)1;
}

//------------------------------------------------------------------------------
void
printCfg()
{
	printf("using configuration:\n");
	printf("\tdirOut           : %s\n",        cfg.dirOut);
	printf("\tprimeMin         : %"PRIu64"\n", cfg.primeMin);
	printf("\tprimeMax         : %"PRIu64"\n", cfg.primeMax);
	printf("\tthrCnt           : %d\n",        cfg.thrCnt);
	printf("\tthrPrimeLoops    : %d\n",        cfg.thrPrimeLoops);
	printf("\tthrPrimesPerLoop : %d\n",        cfg.thrPrimesPerLoop);
	printf("\n");
	fflush(stdout);
}

//------------------------------------------------------------------------------
void
printHelpAndExit()
{
	printf(
		"\n"
		"primegen: find and output prime numbers to a binary file\n"
		"options:\n"
		"\n\t" "-h: help"
		"\n\t" "-o: output directory.       default: %s"
		"\n\t" "-b: prime range begin.      default: %"PRIu64""
		"\n\t" "-e: prime range end.        default: %"PRIu64""
		"\n\t" "-t: thread count.           default: %d"
		"\n\t" "-l: thread prime loops.     default: %d"
		"\n\t" "-p: thread primes per loop. default: %d"
		"\n\n",
		cfg.dirOut,
		cfg.primeMin,
		cfg.primeMax,
		cfg.thrCnt,
		cfg.thrPrimeLoops,
		cfg.thrPrimesPerLoop
	);
	exit(1);
}

//------------------------------------------------------------------------------
void
cliOptsToCfg(int argc, char *argv[])
{
  int  opt;
  bool hasErr = false;

  while ((opt = getopt(argc, argv, ":h:o:b:e:t:l:p:")) != -1)
  {
    switch(opt)
    {
			case 'h':
		    printHelpAndExit();
		    break;
			case 'o':
		    cfg.dirOut = optarg;
		    break;
			case 'b':
				cfg.primeMin = strtoull(optarg, NULL, 10);
		    break;
			case 'e':
				cfg.primeMax = strtoull(optarg, NULL, 10);
		    break;
			case 't':
				cfg.thrCnt = strtol(optarg, NULL, 10);
		    break;
			case 'l':
				cfg.thrPrimeLoops = strtol(optarg, NULL, 10);
		    break;
			case 'p':
				cfg.thrPrimesPerLoop = strtol(optarg, NULL, 10);
		    break;
			default:
				hasErr = true;
		    break;
    }
  }

  // other unparsed options
  for(; optind < argc; optind++){
    hasErr = true;
  }

  if (hasErr) {
  	printf("invalid options given.\n");
    printHelpAndExit();
  }
}

//==============================================================================
int
main(int argc, char *argv[])
{
	cliOptsToCfg(argc, argv);
	printCfg();

  int             ret;
  void           *thrRet;
  thrInfo_st     *thrInfo;
  pthread_attr_t  thrAttr;

  rand_init();

  if (0 != (ret = pthread_attr_init(&thrAttr))) {
	  perror("pthr_attr_init");
	  exit(1);
  }

  if (NULL == (thrInfo = calloc(cfg.thrCnt, sizeof(*thrInfo)))) {
	  perror("calloc");
	  exit(1);
  }

  for (int thrId = 0; thrId < cfg.thrCnt; ++thrId) {
	  thrInfo[thrId].thrNum = thrId;
	  if (0 != (ret = pthread_create(&thrInfo[thrId].thrId, &thrAttr,
	                                 &thrRun, &thrInfo[thrId]))) {
		  perror("pthr_create");
		  exit(1);
	  }
  }

  if (0 != (ret = pthread_attr_destroy(&thrAttr))) {
	  perror("pthr_attr_destroy");
	  exit(1);
  }

  for (int thrId = 0; thrId < cfg.thrCnt; ++thrId) {
	  if (0 != (ret = pthread_join(thrInfo[thrId].thrId, &thrRet))) {
		  perror("pthr_join");
		  exit(1);
	  }
	  printf("thread %d completed\n", thrInfo[thrId].thrNum);
  }

  return 0;
}
