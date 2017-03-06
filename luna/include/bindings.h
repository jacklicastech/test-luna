#ifndef BINDINGS_H
#define BINDINGS_H

#ifdef __cplusplus
extern "C" {
#endif

  int lua_run_file(const char *filename);
  int lua_run_script(const char *script);

#ifdef __cplusplus
}
#endif

#endif // BINDINGS_H
