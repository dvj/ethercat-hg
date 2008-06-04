/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <getopt.h>

#include <iostream>
#include <string>
using namespace std;

#include "Master.h"

/*****************************************************************************/

#define DEFAULT_MASTER 0
#define DEFAULT_COMMAND "slaves"
#define DEFAULT_SLAVEPOSITION -1
#define DEFAULT_DOMAININDEX -1

static unsigned int masterIndex = DEFAULT_MASTER;
static int slavePosition = DEFAULT_SLAVEPOSITION;
static int domainIndex = DEFAULT_DOMAININDEX;
static string command = DEFAULT_COMMAND;

/*****************************************************************************/

void printUsage()
{
    cerr
        << "Usage: ethercat <COMMAND> [OPTIONS]" << endl
		<< "Commands:" << endl
        << "  domain             Show domain information." << endl
        << "  list (ls, slaves)  List all slaves (former 'lsec')." << endl
        << "  pdos               List Pdo mapping of given slaves." << endl
        << "  xml                Generate slave information xml." << endl
		<< "Global options:" << endl
        << "  --master  -m <master>  Index of the master to use. Default: "
		<< DEFAULT_MASTER	<< endl
        << "  --slave   -s <index>   Positive numerical ring position,"
        << endl
        << "                         or 'all' for all slaves. Default: 'all'."
        << endl
        << "  --domain  -d <index>   Positive numerical index,"
        << endl
        << "                         or 'all' for all domains. Default: "
		<< "'all'." << endl
        << "  --help    -h           Show this help." << endl;
}

/*****************************************************************************/

void getOptions(int argc, char **argv)
{
    int c, argCount, optionIndex, number;
	char *remainder;

    static struct option longOptions[] = {
        //name,    has_arg,           flag, val
        {"master", required_argument, NULL, 'm'},
        {"slave",  required_argument, NULL, 's'},
        {"domain", required_argument, NULL, 'd'},
        {"help",   no_argument,       NULL, 'h'},
        {}
    };

    do {
        c = getopt_long(argc, argv, "m:s:d:h", longOptions, &optionIndex);

        switch (c) {
            case 'm':
                number = strtoul(optarg, &remainder, 0);
                if (remainder == optarg || *remainder || number < 0) {
                    cerr << "Invalid master number " << optarg << "!" << endl;
                    printUsage();
                    exit(1);
                }
				masterIndex = number;
                break;

            case 's':
                if (!strcmp(optarg, "all")) {
                    slavePosition = -1;
                } else {
                    number = strtoul(optarg, &remainder, 0);
                    if (remainder == optarg || *remainder
                            || number < 0 || number > 0xFFFF) {
                        cerr << "Invalid slave position "
                            << optarg << "!" << endl;
                        printUsage();
                        exit(1);
                    }
                    slavePosition = number;
                }
                break;

            case 'd':
                if (!strcmp(optarg, "all")) {
                    domainIndex = -1;
                } else {
                    number = strtoul(optarg, &remainder, 0);
                    if (remainder == optarg || *remainder || number < 0) {
                        cerr << "Invalid domain index "
							<< optarg << "!" << endl;
                        printUsage();
                        exit(1);
                    }
                    domainIndex = number;
                }
                break;

            case 'h':
            case '?':
                printUsage();
                exit(0);

            default:
                break;
        }
    }
    while (c != -1);

	argCount = argc - optind;

	if (!argCount) {
        cerr << "Please specify a command!" << endl;
		printUsage();
        exit(1);
	}

    command = argv[optind];
}

/****************************************************************************/

int main(int argc, char **argv)
{
    Master master;
    
	getOptions(argc, argv);

    try {
        master.open(masterIndex);

        if (command == "domain") {
            master.showDomains(domainIndex);
		} else if (command == "list" || command == "ls" || command == "slaves") {
            master.listSlaves();
        } else if (command == "pdos") {
            master.listPdos(slavePosition);
        } else if (command == "xml") {
            master.generateXml(slavePosition);
        } else {
            cerr << "Unknown command " << command << "!" << endl;
            printUsage();
            exit(1);
        }
    } catch (MasterException &e) {
        cerr << e.what() << endl;
        exit(1);
    }

	return 0;
}

/****************************************************************************/
