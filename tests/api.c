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
    gk_free((void **)&line, LTERM);
    return 10;
  }
  gk_free((void **)&line, LTERM);

  gk_optind = 1;
  gk_opterr = 0;
  gk_optopt = '?';
  if (gk_getopt(3, argv, (char *)"n:") != 'n')
    return 11;
  if (gk_optarg == NULL || gk_optarg[0] != '7')
    return 12;

  gk_cur_jbufs = -1;
  jump_buffers = gk_jbufs;
  jump_buffer = &gk_jbuf;
  if (jump_buffers == NULL || jump_buffer == NULL)
    return 13;
  return gk_cur_jbufs == -1 ? 0 : 14;
}
