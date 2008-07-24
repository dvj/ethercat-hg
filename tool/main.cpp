/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <getopt.h>
#include <libgen.h> // basename()

#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandAlias.h"
#include "CommandConfig.h"
#include "CommandData.h"
#include "CommandDebug.h"
#include "CommandDomains.h"
#include "CommandMaster.h"
#include "CommandPdos.h"
#include "CommandSdos.h"
#include "CommandDownload.h"
#include "CommandUpload.h"
#include "CommandSlaves.h"
#include "CommandSiiRead.h"
#include "CommandSiiWrite.h"
#include "CommandStates.h"
#include "CommandXml.h"

/*****************************************************************************/

typedef list<Command *> CommandList;
CommandList commandList;

MasterDevice masterDev;

string binaryBaseName;
string commandName;
Command::StringVector commandArgs;

// option variables
unsigned int masterIndex = 0;
int slavePosition = -1;
int domainIndex = -1;
string dataTypeStr;
Command::Verbosity verbosity = Command::Normal;
bool force = false;
bool helpRequested = false;

/*****************************************************************************/

void printUsage()
{
    CommandList::const_iterator ci;
    size_t maxWidth = 0;

    for (ci = commandList.begin(); ci != commandList.end(); ci++) {
        if ((*ci)->getName().length() > maxWidth) {
            maxWidth = (*ci)->getName().length();
        }
    }

    cerr
        << "Usage: " << binaryBaseName << " <COMMAND> [OPTIONS] [ARGUMENTS]"
        << endl << endl
		<< "Commands (can be abbreviated):" << endl;

    cerr << left;
    for (ci = commandList.begin(); ci != commandList.end(); ci++) {
        cerr << "  " << setw(maxWidth) << (*ci)->getName()
            << "  " << (*ci)->getBriefDescription() << endl;
    }

    cerr
        << endl
		<< "Global options:" << endl
        << "  --master  -m <master>  Index of the master to use. Default: 0."
		<< endl
        << "  --force   -f           Force a command." << endl
        << "  --quiet   -q           Output less information." << endl
        << "  --verbose -v           Output more information." << endl
        << "  --help    -h           Show this help." << endl
        << endl
        << "Numerical values can be specified either with decimal "
        << "(no prefix)," << endl
        << "octal (prefix '0') or hexadecimal (prefix '0x') base." << endl
        << endl
        << "Call '" << binaryBaseName
        << " <COMMAND> --help' for command-specific help." << endl
        << endl
        << "Send bug reports to " << PACKAGE_BUGREPORT << "." << endl;
}

/*****************************************************************************/

void getOptions(int argc, char **argv)
{
    int c, argCount, optionIndex, number;
	char *remainder;

    static struct option longOptions[] = {
        //name,     has_arg,           flag, val
        {"master",  required_argument, NULL, 'm'},
        {"slave",   required_argument, NULL, 's'},
        {"domain",  required_argument, NULL, 'd'},
        {"type",    required_argument, NULL, 't'},
        {"force",   no_argument,       NULL, 'f'},
        {"quiet",   no_argument,       NULL, 'q'},
        {"verbose", no_argument,       NULL, 'v'},
        {"help",    no_argument,       NULL, 'h'},
        {}
    };

    do {
        c = getopt_long(argc, argv, "m:s:d:t:fqvh", longOptions, &optionIndex);

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

            case 't':
                dataTypeStr = optarg;
                break;

            case 'f':
                force = true;
                break;

            case 'q':
                verbosity = Command::Quiet;
                break;

            case 'v':
                verbosity = Command::Verbose;
                break;

            case 'h':
                helpRequested = true;
                break;

            case '?':
                printUsage();
                exit(1);

            default:
                break;
        }
    }
    while (c != -1);

	argCount = argc - optind;

    if (!argCount) {
        if (!helpRequested) {
            cerr << "Please specify a command!" << endl;
        }
        printUsage();
        exit(!helpRequested);
	}

    commandName = argv[optind];
    while (++optind < argc)
        commandArgs.push_back(string(argv[optind]));
}

/****************************************************************************/

list<Command *> getMatchingCommands(const string &cmdStr)
{
    CommandList::iterator ci;
    list<Command *> res;

    // find matching commands from beginning of the string
    for (ci = commandList.begin(); ci != commandList.end(); ci++) {
        if ((*ci)->matchesSubstr(cmdStr)) {
            res.push_back(*ci);
        }
    }

    if (!res.size()) { // nothing found
        // find /any/ matching commands
        for (ci = commandList.begin(); ci != commandList.end(); ci++) {
            if ((*ci)->matchesAbbrev(cmdStr)) {
                res.push_back(*ci);
            }
        }
    }

    return res;
}

/****************************************************************************/

int main(int argc, char **argv)
{
    int retval = 0;
    list<Command *> matchingCommands;
    list<Command *>::const_iterator ci;
    Command *cmd;

    binaryBaseName = basename(argv[0]);

    commandList.push_back(new CommandAlias());
    commandList.push_back(new CommandConfig());
    commandList.push_back(new CommandData());
    commandList.push_back(new CommandDebug());
    commandList.push_back(new CommandDomains());
    commandList.push_back(new CommandDownload());
    commandList.push_back(new CommandMaster());
    commandList.push_back(new CommandPdos());
    commandList.push_back(new CommandSdos());
    commandList.push_back(new CommandSiiRead());
    commandList.push_back(new CommandSiiWrite());
    commandList.push_back(new CommandSlaves());
    commandList.push_back(new CommandStates());
    commandList.push_back(new CommandUpload());
    commandList.push_back(new CommandXml());

	getOptions(argc, argv);

    matchingCommands = getMatchingCommands(commandName);
    masterDev.setIndex(masterIndex);

    if (matchingCommands.size()) {
        if (matchingCommands.size() == 1) {
            cmd = matchingCommands.front();
            if (!helpRequested) {
                try {
                    cmd->execute(masterDev, commandArgs);
                } catch (InvalidUsageException &e) {
                    cerr << e.what() << endl << endl;
                    cerr << cmd->helpString();
                    retval = 1;
                } catch (CommandException &e) {
                    cerr << e.what() << endl;
                    retval = 1;
                } catch (MasterDeviceException &e) {
                    cerr << e.what() << endl;
                    retval = 1;
                }
            } else {
                cout << cmd->helpString();
            }
        } else {
            cerr << "Ambiguous command abbreviation! Matching:" << endl;
            for (ci = matchingCommands.begin();
                    ci != matchingCommands.end();
                    ci++) {
                cerr << (*ci)->getName() << endl;
            }
            cerr << endl;
            printUsage();
            retval = 1;
        }
    } else {
        cerr << "Unknown command " << commandName << "!" << endl << endl;
        printUsage();
        retval = 1;
    }

	return retval;
}

/****************************************************************************/
