#ifndef _LIBINFO_H_
#define _LIBINFO_H_

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sqlite3.h>
#include <errno.h>
#include <time.h>

typedef struct _info {
	/* Key from info */
	char *key;
	/* Value from info */
	char *value;
	/* Timestamp of the record */
	long ts;
	/* Info tag */
	char *tag;
	/*Previous entry*/
	struct _info *prev;
	/*Next entry*/
	struct _info *next;
} info;

int get_max_length(sqlite3 *db);
int putinfo(sqlite3 *db, char *key, char *value, char *tag);
int get_result(info *result, int argc, char **argv, char **azColName);
void print_results(info *result, bool all);
int getinfo(sqlite3 *db, char *key, char *tag);
int check_db(char *dbfile);
sqlite3 * open_db(char *dbfile);
/*
 * cleanup:
 *
 * Cleanup database history from the given timestamp
 *
 * */
int cleanup(sqlite3 *db, struct timespec timestamp);
sqlite3 * init_db(char *dbfile);

/*
int  getinfo(char *nom, char* valeur)
int  putinfo(char *nom, char* valeur)
*/

#define _log(fmt, ...) \
        do { if (debug) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); } while (0)

extern int debug;
#endif

/*vim:noexpandtab:ts=8:sw=8*/
