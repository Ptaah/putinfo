#define _GNU_SOURCE

#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include "info.h"




/*
 * TODO:
 * - main: Gestion de l'initialisation de la base de donnée sqlite
 *   . Vérifier le schéma de la base
 *
 * - putinfo: Meilleurs résolution du timestamp en utilisation clock_gettime.
 *
 * - main: Vérifier la compatibilité avec l'API existante.
 *
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
	printf("%s [-a] [-d <database>] [-e] [-r] variable [value]\n"
	       "    -a --all:            Display all variables.\n"
	       "    -d --database=<arg>: Database file (default "DBFILE" or $DBINFO).\n"
	       "    -D --debug:          Show debug messages.\n"
	       "    -e --explain:        Explain what will be done.\n"
	       "    -h --help:           This help message.\n"
	       "    -r --reset[=date]:   Reset all variables (only until date if given)\n"
	       "                         multiple format are supported:\n"
	       "                         * seconds since EPOC (date +%%s)\n"
	       "                         * date Year-Month-Day (date +%%F)\n"
	       "                         * date and time 'Y/M/D HH:MM:SS' (date +%%X)\n"
	       "    -t --tag=<tag>:      Tag to identify variable type.\n", argv[0]);
}

/* 
 * convert_timestamp
 *
 * convert timestamp argument to seconds since EPOC
 * */

time_t convert_timestamp(char *timestamp){
	char *res;
	struct tm ts_to_tm;

	/*If timestamp is a date*/
	if ((res = strptime(options.reset_timestamp, "%F", &ts_to_tm)) != NULL){
		if (*res == '\0'){
			fprintf(stderr, "convert_timestamp: date detected\n");
			return mktime(&ts_to_tm);
		}
	}
	/*If timestamp is a date and a time*/
	if ((res = strptime(options.reset_timestamp, "%F %X", &ts_to_tm)) != NULL){
		if (*res == '\0'){
			fprintf(stderr, "convert_timestamp: date and time detected\n");
			return mktime(&ts_to_tm);
		}
	}
	/*If timestamp is second since EPOC */
	if ((res = strptime(timestamp, "%s", &ts_to_tm)) != NULL){
		if (*res == '\0'){
			fprintf(stderr, "convert_timestamp: seconds detected\n");
			return mktime(&ts_to_tm);
		}
	}
	errno = EINVAL;
	return -1;
}

int main(int argc, char **argv) {
	char opt;
	char *dbfile = NULL;
	sqlite3 *db = NULL;
	struct timespec ts;
	/*
	 * Options à gérer:
	 * -a: Donne l'ensemble des clefs/valeur de la base de donnée (getinfo)
	 * -d <arg>: Répertoire info à utiliser (gestion du dbinfo) (GLOBAL_TMPDIR ou /tmp + SLURM_JOB_ID) Si ENVIRONMENT=BATCH
	 * -e: ecriture sur stderr du résultat de la commande (putinfo)
	 * -r: purge complète si pas d'argument, et RAZ de la variable si un argument (reset)
	 * */

	static struct option long_options[] =
	{
	    {"all", no_argument, NULL, 'a'},
	    {"debug", optional_argument, NULL, 'D'},
	    {"dir", required_argument, NULL, 'd'},
	    {"explain", no_argument, NULL, 'e'},
	    {"help", no_argument, NULL, 'h'},
	    {"reset", optional_argument, NULL, 'r'},
	    {"tag", required_argument, NULL, 't'},
	    {NULL, 0, NULL, 0}
	};

	while ((opt = getopt_long(argc, argv, "ad:Dert:h", long_options, NULL)) != -1)
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
		 // short option 'e'
		 case 'e':
		     options.explain = true;
		     break;
		 // short option 'h'
		 case 'h':
		     help(argv);
		     exit(0);
		     break;
		 // short option 'r'
		 case 'r':
		     options.reset = true;
		     options.reset_timestamp = optarg;
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
	if (optind < argc)
		options.value = argv[optind++];
	else
		options.value = NULL;
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
		if (options.value){
			if (options.explain)
				fprintf(stderr, "%s = %s", options.variable, options.value);
			putinfo(db, options.variable, options.value, options.tag);
		} else {
			getinfo(db, options.variable, options.tag);
		}
	} else {
		if (options.all)
			getinfo(db, "%", options.tag);
		if (options.reset) {
			if (options.reset_timestamp != NULL){
				ts.tv_sec = convert_timestamp(options.reset_timestamp);
				if (ts.tv_sec == -1){
					perror("main: timestamp format");
					return 1;
				}
			} else {
				if (clock_gettime(CLOCK_REALTIME, &ts)) {
					perror("main: Unable to get time");
					return 1;
				}
			}
			cleanup(db, ts);
		}
	}

	sqlite3_close(db);
	return 0;
}

// vim:noexpandtab:ts=8:sw=8
