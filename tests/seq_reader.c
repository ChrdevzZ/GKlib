#include <GKlib.h>

static const char *valid_header =
    "A R N D C Q E G H I L K M F P S T W Y V\n";


static int expect_read_failure(char *filename)
{
  gk_seq_t *seq;

  seq = gk_seq_ReadGKMODPSSM(filename);
  if (seq != NULL) {
    gk_seq_free(seq);
    return 0;
  }

  return gk_GetCurMemoryUsed() == 0;
}


static int expect_signal_failure(char *filename)
{
  volatile int signum=0;

  if (!gk_sigtrap())
    return 0;
  gk_set_exit_on_error(1);
  errno = 0;
  switch (gk_sigcatch()) {
    case 0:
      (void)gk_seq_ReadGKMODPSSM(filename);
      break;
    case SIGERR:
      signum = SIGERR;
      break;
    default:
      signum = SIGMEM;
      break;
  }
  if (!gk_siguntrap())
    return 0;
  gk_set_exit_on_error(0);

  return signum == SIGERR && errno == EINVAL &&
      gk_GetCurMemoryUsed() == 0;
}


int main(int argc, char *argv[])
{
  FILE *file;
  gk_seq_t *seq;
  int i;

  if (argc != 2)
    return 2;

  file = fopen(argv[1], "wb");
  if (file == NULL)
    return 3;

  if (fprintf(file, "%s1 A", valid_header) < 0)
    return 4;
  for (i=0; i<40; i++) {
    if (fprintf(file, " %d", i) < 0)
      return 5;
  }
  if (fclose(file) != 0)
    return 6;

  seq = gk_seq_ReadGKMODPSSM(argv[1]);
  if (seq == NULL || seq->len != 1 || seq->sequence[0] != 0 ||
      seq->pssm[0][0] != 0 || seq->psfm[0][0] != 20)
    return 7;

  gk_seq_free(seq);

  file = fopen(argv[1], "wb");
  if (file == NULL)
    return 8;
  if (fputs("A R N D C Q E G H I L K M F P S T W Y V", file) == EOF ||
      fputc('\0', file) == EOF || fputs(" hidden\n", file) == EOF ||
      fclose(file) != 0)
    return 9;

  gk_set_exit_on_error(0);
  if (!gk_malloc_init())
    return 10;
  errno = 0;
  if (gk_seq_ReadGKMODPSSM(NULL) != NULL || errno != EINVAL)
    return 15;
  if (!expect_signal_failure(NULL))
    return 23;
  if (!expect_read_failure(argv[1]))
    return 11;

  file = fopen(argv[1], "wb");
  if (file == NULL ||
      fputs("A R N D C Q E G H I L K M F P S T W Y A\n", file) == EOF ||
      fclose(file) != 0 || !expect_read_failure(argv[1]))
    return 12;

  file = fopen(argv[1], "wb");
  if (file == NULL || fprintf(file, "%s1 A 0\n", valid_header) < 0 ||
      fclose(file) != 0 || !expect_read_failure(argv[1]))
    return 13;

  file = fopen(argv[1], "wb");
  if (file == NULL ||
      fprintf(file, "%s1 A 2147483648\n", valid_header) < 0 ||
      fclose(file) != 0 || !expect_read_failure(argv[1]))
    return 14;

  file = fopen(argv[1], "wb");
  if (file == NULL || fprintf(file, "%s1 A ", valid_header) < 0)
    return 18;
  for (i=0; i<256; i++) {
    if (fputc('9', file) == EOF) {
      fclose(file);
      return 19;
    }
  }
  if (fputc('\n', file) == EOF || fclose(file) != 0)
    return 20;
  errno = 0;
  if (!expect_read_failure(argv[1]) || errno != EINVAL)
    return 21;
  if (!expect_signal_failure(argv[1]))
    return 22;

  file = fopen(argv[1], "wb");
  if (file == NULL || fprintf(file, "%s1 A", valid_header) < 0)
    return 15;
  for (i=0; i<41; i++) {
    if (fprintf(file, " %d", i) < 0)
      return 16;
  }
  if (fclose(file) != 0 || !expect_read_failure(argv[1]))
    return 17;

  gk_malloc_cleanup(0);
  gk_set_exit_on_error(1);
  remove(argv[1]);

  return 0;
}
