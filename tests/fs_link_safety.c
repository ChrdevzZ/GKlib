#include <GKlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif


static int write_sentinel(const char *filename)
{
  FILE *stream;

  stream = fopen(filename, "wb");
  if (stream == NULL)
    return 0;
  if (fwrite("keep", 1, 4, stream) != 4) {
    fclose(stream);
    return 0;
  }
  return fclose(stream) == 0;
}


static int create_directory_link(const char *linkname, const char *target)
{
#ifdef _WIN32
  DWORD error;

  if (CreateSymbolicLinkA(linkname, target,
      SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
    return 1;
  error = GetLastError();
  if (error == ERROR_INVALID_PARAMETER &&
      CreateSymbolicLinkA(linkname, target, SYMBOLIC_LINK_FLAG_DIRECTORY))
    return 1;
  if (error == ERROR_PRIVILEGE_NOT_HELD || error == ERROR_INVALID_PARAMETER ||
      error == ERROR_NOT_SUPPORTED) {
    printf("GKLIB_LINK_TEST_SKIPPED: directory links are unavailable\n");
    return 0;
  }
  return -1;
#else
  if (symlink(target, linkname) == 0)
    return 1;
  if (errno == EPERM || errno == ENOSYS) {
    printf("GKLIB_LINK_TEST_SKIPPED: symbolic links are unavailable\n");
    return 0;
  }
  return -1;
#endif
}


int main(void)
{
  const char *parent_target;
  int link_status;

#ifdef _WIN32
  parent_target = "..\\gklib-link-target";
#else
  parent_target = "../gklib-link-target";
#endif

  if (gk_mkpath((char *)"gklib-link-target/child") != 0 ||
      !write_sentinel("gklib-link-target/child/keep.txt"))
    return 1;

  link_status = create_directory_link("gklib-link-leaf", "gklib-link-target");
  if (link_status <= 0) {
    gk_rmpath((char *)"gklib-link-target");
    return link_status == 0 ? 0 : 2;
  }

  if (gk_rmpath((char *)"gklib-link-leaf") != 0 ||
      !gk_fexists((char *)"gklib-link-target/child/keep.txt")) {
    gk_rmpath((char *)"gklib-link-leaf");
    gk_rmpath((char *)"gklib-link-target");
    return 3;
  }

  /* A link in an explicit parent component must not be traversed either. */
  link_status = create_directory_link("gklib-link-parent",
      "gklib-link-target");
  if (link_status != 1 ||
      gk_rmpath((char *)"gklib-link-parent/child") == 0 ||
      !gk_fexists((char *)"gklib-link-target/child/keep.txt") ||
      gk_rmpath((char *)"gklib-link-parent") != 0) {
    gk_rmpath((char *)"gklib-link-parent");
    gk_rmpath((char *)"gklib-link-target");
    return 4;
  }

  /* A link nested inside a recursively deleted tree must remain a leaf. */
  if (gk_mkpath((char *)"gklib-link-tree/child") != 0 ||
      !write_sentinel("gklib-link-tree/child/local.txt")) {
    gk_rmpath((char *)"gklib-link-target");
    return 5;
  }
  link_status = create_directory_link("gklib-link-tree/escape",
      parent_target);
  if (link_status != 1 || gk_rmpath((char *)"gklib-link-tree") != 0 ||
      !gk_fexists((char *)"gklib-link-target/child/keep.txt")) {
    gk_rmpath((char *)"gklib-link-tree");
    gk_rmpath((char *)"gklib-link-target");
    return 6;
  }

  if (gk_rmpath((char *)"gklib-link-target") != 0)
    return 7;
  return 0;
}
