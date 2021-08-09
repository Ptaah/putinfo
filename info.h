#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	/* Info type */
	const char* type;
	/* Id */
	long id;
	/*Previous entry*/
	struct _info *prev;
	/*Next entry*/
	struct _info *next;
} info;

int get_max_length(sqlite3 *db);
int putinfo(sqlite3 *db, int id, char *key, char *value, char *type);
int get_result(info *result, int argc, char **argv, char **azColName);
void print_results(info *result);
int getinfo(sqlite3 *db, int id, char *key, char *type);
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

/*vim:noexpandtab:ts=8:sw=8*/
