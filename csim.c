#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void printSummary(int hits, int misses, int evictions)
{
  printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
  FILE *output_fp = fopen(".csim_results", "w");
  assert(output_fp);
  fprintf(output_fp, "%d %d %d\n", hits, misses, evictions);
  fclose(output_fp);
}

void printHelp(const char *name)
{
  printf(
      "Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n"
      "Options:\n"
      "  -h         Print this help message.\n"
      "  -v         Optional verbose flag.\n"
      "  -s <num>   Number of set index bits.\n"
      "  -E <num>   Number of lines per set.\n"
      "  -b <num>   Number of block offset bits.\n"
      "  -t <file>  Trace file.\n\n"
      "Examples:\n"
      "  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n"
      "  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n",
      name, name, name);
}

typedef struct
{
  uint64_t tag;
  uint64_t last_used;
  unsigned char valid;
  unsigned char dirty;
} CacheLine;

typedef struct
{
  CacheLine *lines;
  size_t ways;
  unsigned set_bits;
  unsigned block_bits;
  uint64_t set_mask;
  uint64_t clock;
  uint64_t hits;
  uint64_t misses;
  uint64_t evictions;
  int verbose;
} Cache;

static int parse_number(const char *text, uint64_t *value)
{
  char *end;
  if (*text == '\0' || *text == '-')
    return 0;
  errno = 0;
  unsigned long long parsed = strtoull(text, &end, 10);
  if (errno != 0 || *end != '\0')
    return 0;
  *value = parsed;
  return 1;
}

/* Each trace access refers to the cache block containing its starting address.
 * The access size and the optional register number do not affect h/m/e.
 * Both loads and stores allocate on a miss; dirty eviction is a write-back,
 * not a second CPU access, so it must not increment h/m/e again. */
static void access_cache(Cache *cache, uint64_t address, int store)
{
  size_t set = (size_t)((address >> cache->block_bits) & cache->set_mask);
  unsigned tag_shift = cache->set_bits + cache->block_bits;
  uint64_t tag = tag_shift == 64 ? 0 : address >> tag_shift;
  CacheLine *lines = cache->lines + set * cache->ways;
  size_t empty = cache->ways;
  size_t oldest = 0;
  ++cache->clock;

  for (size_t i = 0; i < cache->ways; ++i)
  {
    if (lines[i].valid && lines[i].tag == tag)
    {
      ++cache->hits;
      lines[i].last_used = cache->clock;
      if (store)
        lines[i].dirty = 1;
      if (cache->verbose)
        printf(" hit");
      return;
    }
    if (!lines[i].valid && empty == cache->ways)
      empty = i;
    if (lines[i].last_used < lines[oldest].last_used)
      oldest = i;
  }

  ++cache->misses;
  if (cache->verbose)
    printf(" miss");
  size_t victim = empty;
  if (empty == cache->ways)
  {
    victim = oldest;
    ++cache->evictions;
    if (cache->verbose)
      printf(" eviction");
  }
  lines[victim] = (CacheLine){tag, cache->clock, 1, (unsigned char)store};
}

int main(int argc, char *argv[])
{
  uint64_t s = 0, ways = 0, b = 0;
  int have_s = 0, have_ways = 0, have_b = 0, verbose = 0;
  const char *trace_path = NULL;
  int option;
  while ((option = getopt(argc, argv, "hvs:E:b:t:")) != -1)
  {
    switch (option)
    {
    case 'h':
      printHelp(argv[0]);
      return 0;
    case 'v':
      verbose = 1;
      break;
    case 's':
      have_s = parse_number(optarg, &s);
      if (!have_s)
        goto invalid_options;
      break;
    case 'E':
      have_ways = parse_number(optarg, &ways);
      if (!have_ways)
        goto invalid_options;
      break;
    case 'b':
      have_b = parse_number(optarg, &b);
      if (!have_b)
        goto invalid_options;
      break;
    case 't':
      trace_path = optarg;
      break;
    default:
      goto invalid_options;
    }
  }
  if (!have_s || !have_ways || !have_b || !trace_path || optind != argc ||
      ways == 0 || s >= sizeof(size_t) * CHAR_BIT || b >= 64 || s + b > 64)
    goto invalid_options;

  size_t sets = (size_t)1 << s;
  if (ways > SIZE_MAX / sizeof(CacheLine) / sets)
  {
    fprintf(stderr, "Cache configuration is too large.\n");
    return 1;
  }
  Cache cache = {0};
  cache.ways = (size_t)ways;
  cache.set_bits = (unsigned)s;
  cache.block_bits = (unsigned)b;
  cache.set_mask = (uint64_t)sets - 1;
  cache.verbose = verbose;
  cache.lines = calloc(sets * cache.ways, sizeof(CacheLine));
  if (!cache.lines)
  {
    fprintf(stderr, "Cannot allocate the requested cache.\n");
    return 1;
  }
  FILE *trace = fopen(trace_path, "r");
  if (!trace)
  {
    perror(trace_path);
    free(cache.lines);
    return 1;
  }

  char line[256];
  size_t line_number = 0;
  int status = 0;
  while (fgets(line, sizeof(line), trace))
  {
    ++line_number;
    char operation;
    uint64_t address;
    unsigned size;
    if (sscanf(line, " %c", &operation) != 1 || operation == '#')
      continue;
    if (operation == 'I')
      continue;
    if (sscanf(line, " %c %" SCNx64 ",%u", &operation, &address, &size) != 3 ||
        size == 0 || (operation != 'L' && operation != 'S' && operation != 'M'))
    {
      fprintf(stderr, "Invalid trace at line %zu.\n", line_number);
      status = 1;
      break;
    }
    if (verbose)
      printf("%c %" PRIx64 ",%u", operation, address, size);
    access_cache(&cache, address, operation == 'S');
    if (operation == 'M')
      access_cache(&cache, address, 1);
    if (verbose)
      putchar('\n');
  }
  if (ferror(trace))
  {
    perror("Reading trace");
    status = 1;
  }
  fclose(trace);
  free(cache.lines);
  if (cache.hits > INT_MAX || cache.misses > INT_MAX || cache.evictions > INT_MAX)
  {
    fprintf(stderr, "Counters exceed the printSummary interface limit.\n");
    return 1;
  }
  if (status == 0)
    printSummary((int)cache.hits, (int)cache.misses, (int)cache.evictions);
  return status;

invalid_options:
  fprintf(stderr, "Invalid or missing cache options. Use -h for help.\n");
  return 1;
}
