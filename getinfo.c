#define _GNU_SOURCE

#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include "info.h"

#define DBFILE "/tmp/base.sqlite"



/*
 * TODO:
 * - global: support d'etcd pour éviter des appels au système de fichiers
 * */

struct _options {
    char *database;
    bool all;
    bool reset;
    char *reset_timestamp;
    bool explain;
    char *tag;
    char *variable;
    char *value;
} options;

void help(char **argv){
	printf("%s [-a] [-d <database>] [-e] variable\n"
	       "    -a --all:            Display all variables.\n"
	       "    -d --database=<arg>: Database file (default "DBFILE" or $DBINFO).\n"
	       "    -D --debug:          Show debug messages.\n"
	       "    -h --help:           This help message.\n"
	       "    -t --tag=<tag>:      Tag to identify variable type.\n", argv[0]);
}

int main(int argc, char **argv) {
	char opt;
	char *dbfile = NULL;
	sqlite3 *db = NULL;
	/*
	 * Options à gérer:
	 * -a: Donne l'ensemble des clefs/valeur de la base de donnée (getinfo)
	 * -d <arg>: Répertoire info à utiliser (gestion du dbinfo) (GLOBAL_TMPDIR ou /tmp + SLURM_JOB_ID) Si ENVIRONMENT=BATCH
	 * */

	static struct option long_options[] =
	{
	    {"all", no_argument, NULL, 'a'},
	    {"debug", optional_argument, NULL, 'D'},
	    {"dir", required_argument, NULL, 'd'},
	    {"help", no_argument, NULL, 'h'},
	    {"tag", required_argument, NULL, 't'},
	    {NULL, 0, NULL, 0}
	};

	while ((opt = getopt_long(argc, argv, "ad:Dt:h", long_options, NULL)) != -1)
	{
	    // check to see if a single character or long option came through
	    switch (opt)
	    {
		 // short option 'a'
		 case 'a':
		     options.all = true;
		     break;
		 // short option 'd'
		 case 'd':
		     options.database = optarg;
		     break;
		 // short option 'D'
		 case 'D':
		     debug ++;
		     break;
		 // short option 'h'
		 case 'h':
		     help(argv);
		     exit(0);
		     break;
		 // short option 't'
		 case 't':
		     options.tag = optarg;
		     break;
		 default:
		     help(argv);
		     exit(1);
	    }
	}

	if (optind < argc)
		options.variable = argv[optind++];
	else
		options.variable = NULL;
	if (optind < argc){
		fprintf(stderr, "Too many argument.\n");
		help(argv);
		exit(1);
	}

	_log("main: all=%d directory=%s explain=%d reset=%d(%s) variable=%s value=%s\n",
		options.all, options.database, options.explain, options.reset,
		options.reset_timestamp, options.variable, options.value);

	if (options.database == NULL){
		dbfile = info_getdb();
	} else {
		dbfile = options.database;
	}

	_log("main: opening db from: %s\n", dbfile);

	if (! check_db(dbfile)) {
		db = init_db(dbfile);
	} else {
		db = open_db(dbfile);
	}
	if (db == NULL) {
		perror("main: Unable to open db");
		exit(1);
	}

	/*Manage Tag for the entry*/
	if (options.tag == NULL){
		options.tag = info_gettag();
	}
	_log("main: chosen tag: %s\n", options.tag);

	if (options.variable){
		getinfo(db, options.variable, options.tag);
	} else {
		if (options.all)
			getinfo(db, "%", options.tag);
	}

	sqlite3_close(db);
	return 0;
}

// vim:noexpandtab:ts=8:sw=8
