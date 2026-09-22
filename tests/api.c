#include <GKlib.h>

static int check_replacement(void)
{
  char guard = '?';
  char *output[2] = {NULL, &guard};
  int status;
  int valid;

  /* Only the first pointer belongs to the output parameter. An escaped
     character must index its allocated string, not the pointer array. */
  status = gk_strstr_replace((char *)"foo", (char *)"foo",
                            (char *)"a\\$", (char *)"", output);
  valid = status == 2 && guard == '?' && strcmp(output[0], "a$") == 0;
  gk_free((void **)&output[0], LTERM);
  if (!valid)
    return 0;

  status = gk_strstr_replace((char *)"foo foo", (char *)"foo",
                            (char *)"x\\$", (char *)"g", output);
  valid = status == 3 && strcmp(output[0], "x$ x$") == 0;
  gk_free((void **)&output[0], LTERM);
  if (!valid)
    return 0;

  {
    const char *cases[][4] = {
      {"foo", "x*", "", "foo"},
      {"foo", "x*", "-", "-f-o-o-"},
      {"foo", "^", "-", "-foo"},
      {"foo", "$", "-", "foo-"},
      {"", "^$", "-", "-"},
      {"b", "(a)?b", "<$1>", "<>"},
      {"foo foo", "(foo)", "<$1>", "<foo> <foo>"}
    };
    const int counts[] = {5, 5, 2, 2, 2, 2, 3};
    size_t i;

    /* Empty matches, anchors and absent captures must terminate and retain
       every unmatched byte, with the documented substitution count. */
    for (i=0; i<sizeof(counts)/sizeof(counts[0]); i++) {
      status = gk_strstr_replace((char *)cases[i][0], (char *)cases[i][1],
                                (char *)cases[i][2], (char *)"g", output);
      valid = status == counts[i] && strcmp(output[0], cases[i][3]) == 0;
      gk_free((void **)&output[0], LTERM);
      if (!valid)
        return 0;
    }
  }
  return valid;
}


static int check_replacement_invalid_arguments(void)
{
  char *output = (char *)1;
  int valid;

  gk_set_exit_on_error(0);
  valid = gk_strstr_replace(NULL, (char *)"x", (char *)"y",
                           (char *)"", &output) == 0 &&
          output == NULL;
  output = (char *)1;
  valid = valid &&
      gk_strstr_replace((char *)"x", NULL, (char *)"y",
                        (char *)"", &output) == 0 &&
      output == NULL;
  output = (char *)1;
  valid = valid &&
      gk_strstr_replace((char *)"x", (char *)"x", NULL,
                        (char *)"", &output) == 0 &&
      output == NULL;
  output = (char *)1;
  valid = valid &&
      gk_strstr_replace((char *)"x", (char *)"x", (char *)"y",
                        NULL, &output) == 0 &&
      output == NULL;
  valid = valid &&
      gk_strstr_replace((char *)"x", (char *)"x", (char *)"y",
                        (char *)"", NULL) == 0;
  gk_set_exit_on_error(1);

  return valid;
}


static int check_getopt_w(void)
{
  struct gk_option long_options[] = {
    {(char *)"value", no_argument, NULL, 'v'},
    {NULL, 0, NULL, 0}
  };
  char *short_argv[] = {
    (char *)"gklib-api-test", (char *)"-W", (char *)"value", NULL
  };
  char *long_argv[] = {
    (char *)"gklib-api-test", (char *)"-W", (char *)"value", NULL
  };
  char *unknown_argv[] = {
    (char *)"gklib-api-test", (char *)"-W", (char *)"unknown", NULL
  };
  char *missing_argv[] = {
    (char *)"gklib-api-test", (char *)"-W", NULL
  };

  gk_opterr = 0;
  gk_optind = 0;
  if (gk_getopt(3, short_argv, (char *)"W;") != 'W' ||
      gk_optarg != NULL)
    return 0;

  gk_optind = 0;
  if (gk_getopt_long(3, long_argv, (char *)"W;", long_options, NULL) != 'v' ||
      gk_optarg == NULL || strcmp(gk_optarg, "value") != 0)
    return 0;

  gk_optind = 0;
  if (gk_getopt_long(3, unknown_argv, (char *)"W;", long_options, NULL) != 'W' ||
      gk_optarg == NULL || strcmp(gk_optarg, "unknown") != 0)
    return 0;

  gk_optind = 0;
  gk_optopt = 0;
  if (gk_getopt_long(2, missing_argv, (char *)"W;", long_options, NULL) != '?' ||
      gk_optopt != 'W')
    return 0;

  return 1;
}


static int check_memory_tracking(void)
{
  void *first;
  void *second;
  int valid;

  if (!gk_malloc_init())
    return 0;

  first = gk_malloc(31, "first tracked allocation");
  second = gk_malloc(47, "second tracked allocation");
  valid = first != NULL && second != NULL && gk_GetCurMemoryUsed() == 78;
  gk_free(&first, &second, LTERM);
  valid = valid && first == NULL && second == NULL &&
          gk_GetCurMemoryUsed() == 0;
  gk_malloc_cleanup(0);

  return valid;
}


static int check_tokenizer(void)
{
  gk_Tokens_t tokens;

  tokens.ntoks = -1;
  tokens.strbuf = NULL;
  tokens.list = NULL;
  gk_strtokenize((char *)" alpha,,beta ", (char *)", ", &tokens);
  if (tokens.ntoks != 2 || tokens.list == NULL ||
      strcmp(tokens.list[0], "alpha") != 0 ||
      strcmp(tokens.list[1], "beta") != 0) {
    gk_freetokenslist(&tokens);
    return 0;
  }
  gk_freetokenslist(&tokens);
  if (tokens.list != NULL || tokens.strbuf != NULL)
    return 0;

  tokens.ntoks = 7;
  tokens.strbuf = (char *)1;
  tokens.list = (char **)1;
  gk_set_exit_on_error(0);
  gk_strtokenize(NULL, (char *)",", &tokens);
  gk_set_exit_on_error(1);

  return tokens.ntoks == 0 && tokens.strbuf == NULL && tokens.list == NULL;
}


int main(void)
{
  int *values;
  uint32_t random_value;
  regex_t regex;
  gk_csr_t *matrix;
  FILE *stream;
  char *line = NULL;
  size_t line_size = 0;
  char *argv[] = {(char *)"gklib-api-test", (char *)"-n", (char *)"7", NULL};
  jmp_buf *jump_buffers;
  jmp_buf *jump_buffer;

  if (!check_memory_tracking())
    return 16;
  if (!check_tokenizer())
    return 17;

  values = gk_imalloc(3, (char *)"gklib-api-test");
  values[0] = 3;
  values[1] = 1;
  values[2] = 2;
  gk_isorti(3, values);
  if (values[0] != 1 || values[1] != 2 || values[2] != 3)
    return 1;
  if (gk_isum(3, values, 1) != 6)
    return 2;
  gk_free((void **)&values, LTERM);
  if (values != NULL)
    return 3;

  gk_randinit(12345);
  random_value = gk_randint32();
  gk_randinit(12345);
  if (gk_randint32() != random_value)
    return 4;

  if (regcomp(&regex, "^gk(lib)?$", REG_EXTENDED) != 0)
    return 5;
  if (regexec(&regex, "gklib", 0, NULL, 0) != 0) {
    regfree(&regex);
    return 6;
  }
  regfree(&regex);

  if (!check_replacement())
    return 15;
  if (!check_replacement_invalid_arguments())
    return 18;

  matrix = gk_csr_Create();
  if (matrix == NULL)
    return 7;
  gk_csr_Free(&matrix);
  if (matrix != NULL)
    return 8;

  stream = gk_fopen((char *)"gklib-api-test.tmp", (char *)"w",
                    "gklib-api-test");
  fputs("GKlib file IO\n", stream);
  gk_fclose(stream);
  stream = gk_fopen((char *)"gklib-api-test.tmp", (char *)"r",
                    "gklib-api-test");
  if (gk_getline(&line, &line_size, stream) <= 0) {
    gk_fclose(stream);
    remove("gklib-api-test.tmp");
    return 9;
  }
  gk_fclose(stream);
  remove("gklib-api-test.tmp");
  if (strcmp(line, "GKlib file IO\n") != 0) {
    free(line);
    return 10;
  }
  free(line);

  gk_optind = 1;
  gk_opterr = 0;
  gk_optopt = '?';
  if (gk_getopt(3, argv, (char *)"n:") != 'n')
    return 11;
  if (gk_optarg == NULL || gk_optarg[0] != '7')
    return 12;
  if (!check_getopt_w())
    return 13;

  gk_cur_jbufs = -1;
  jump_buffers = gk_jbufs;
  jump_buffer = &gk_jbuf;
  if (jump_buffers == NULL || jump_buffer == NULL)
    return 14;
  return gk_cur_jbufs == -1 ? 0 : 15;
}
