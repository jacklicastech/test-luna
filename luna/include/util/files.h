/* 
 * File:   files.h
 * Author: Colin
 *
 * Created on January 19, 2016, 11:35 PM
 */

#ifndef FILES_H
#define	FILES_H

#ifdef	__cplusplus
extern "C" {
#endif

char *expand_path(const char *filename, const char *dir);
char *find_readable_file(const char *subdir, const char *filename);
char *find_writable_file(const char *subdir, const char *filename);
int rm_rf(const char *dir);
int mkdir_p(const char *dir);
void listdir(const char *name, int level);
void walkdir(const char *name, int level, void (*callback)(const char *path, const char *name, int type, int level, void *arg), void *arg);

#ifdef	__cplusplus
}
#endif

#endif	/* FILES_H */

