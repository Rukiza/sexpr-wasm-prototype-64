#include <assert.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "wasm.h"
#include "wasm-internal.h"
#include "wasm-parse.h"
#include "wasm-gen.h"

enum {
  FLAG_VERBOSE,
  FLAG_HELP,
  FLAG_DUMP_MODULE,
  FLAG_OUTPUT,
  FLAG_SPEC,
  FLAG_SPEC_VERBOSE,
  FLAG_BR_IF,
  NUM_FLAGS
};

static const char* s_infile;
static const char* s_outfile;
static int s_dump_module;
static int s_verbose;
static int s_spec;
static int s_spec_verbose;
static int s_br_if;

static struct option s_long_options[] = {
    {"verbose", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
    {"dump-module", no_argument, NULL, 'd'},
    {"output", no_argument, NULL, 'o'},
    {"spec", no_argument, NULL, 0},
    {"spec-verbose", no_argument, NULL, 0},
    {"br-if", no_argument, NULL, 0},
    {NULL, 0, NULL, 0},
};
STATIC_ASSERT(NUM_FLAGS + 1 == ARRAY_SIZE(s_long_options));

typedef struct OptionHelp {
  int flag;
  const char* metavar;
  const char* help;
} OptionHelp;

static OptionHelp s_option_help[] = {
    {FLAG_VERBOSE, NULL, "use multiple times for more info"},
    {FLAG_DUMP_MODULE, NULL, "print a hexdump of the module to stdout"},
    {FLAG_SPEC, NULL,
     "parse a file with multiple modules and assertions, like the spec tests"},
    {FLAG_SPEC_VERBOSE, NULL, "print logging messages when running spec files"},
    {FLAG_BR_IF, NULL, "use br_if and loops without explicit branches"},
    {NUM_FLAGS, NULL},
};

static void usage(const char* prog) {
  printf("usage: %s [option] filename\n", prog);
  printf("options:\n");
  struct option* opt = &s_long_options[0];
  int i = 0;
  for (; opt->name; ++i, ++opt) {
    OptionHelp* help = NULL;

    int n = 0;
    while (s_option_help[n].help) {
      if (i == s_option_help[n].flag) {
        help = &s_option_help[n];
        break;
      }
      n++;
    }

    if (opt->val) {
      printf("  -%c, ", opt->val);
    } else {
      printf("      ");
    }

    if (help && help->metavar) {
      char buf[100];
      snprintf(buf, 100, "%s=%s", opt->name, help->metavar);
      printf("--%-30s", buf);
    } else {
      printf("--%-30s", opt->name);
    }

    if (help) {
      printf("%s", help->help);
    }

    printf("\n");
  }
  exit(0);
}

static void parse_options(int argc, char** argv) {
  int c;
  int option_index = 0;

  while (1) {
    c = getopt_long(argc, argv, "vhdo:", s_long_options, &option_index);
    if (c == -1) {
      break;
    }

  redo_switch:
    switch (c) {
      case 0:
        c = s_long_options[option_index].val;
        if (c) {
          goto redo_switch;
        }

        switch (option_index) {
          case FLAG_VERBOSE:
          case FLAG_HELP:
          case FLAG_DUMP_MODULE:
          case FLAG_OUTPUT:
            /* Handled above by goto */
            assert(0);
            break;

          case FLAG_SPEC:
            s_spec = 1;
            break;

          case FLAG_SPEC_VERBOSE:
            s_spec_verbose = 1;
            break;

          case FLAG_BR_IF:
            s_br_if = 1;
            break;
        }
        break;

      case 'v':
        s_verbose++;
        break;

      case 'h':
        usage(argv[0]);

      case 'd':
        s_dump_module = 1;
        break;

      case 'o':
        s_outfile = optarg;
        break;

      case '?':
        break;

      default:
        FATAL("getopt_long returned '%c' (%d)\n", c, c);
        break;
    }
  }

  if (optind < argc) {
    s_infile = argv[optind];
  } else {
    FATAL("No filename given.\n");
    usage(argv[0]);
  }
}

int main(int argc, char** argv) {
  parse_options(argc, argv);

  FILE* f = fopen(s_infile, "rb");
  if (!f) {
    FATAL("unable to read %s\n", s_infile);
  }

  fseek(f, 0, SEEK_END);
  size_t fsize = ftell(f);
  fseek(f, 0, SEEK_SET);
  void* data = malloc(fsize);
  if (!data) {
    FATAL("unable to alloc %zd bytes\n", fsize);
  }
  if (fread(data, 1, fsize, f) != fsize) {
    FATAL("unable to read %zd bytes from %s\n", fsize, s_infile);
  }
  fclose(f);

  WasmSource source;
  source.filename = s_infile;
  source.start = data;
  source.end = data + fsize;

  WasmGenOptions options = {};
  options.outfile = s_outfile;
  options.dump_module = s_dump_module;
  options.verbose = s_verbose;
  options.spec = s_spec;
  options.spec_verbose = s_spec_verbose;
  options.br_if = s_br_if;
  int result = wasm_gen_file(&source, &options);
  free(data);
  return result;
}
