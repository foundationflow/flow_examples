#include "boost/lexical_cast.hpp"


#include "flow/Trace.h"
#include "flow/DeterministicRandom.h"
#include "flow/SignalSafeUnwind.h"

#include "fdbclient/NativeAPI.h"

#include "flow/SimpleOpt.h"

#include "helloflow/FlowLineNoise.h"

#include <signal.h>

#ifdef __unixish__
#include <stdio.h>
#include "helloflow/linenoise/linenoise.h"
#endif

#ifndef WIN32
#include "versions.h"
#endif

#include "flow/actorcompiler.h"  // This must be the last #include.

enum { OPT_CONNFILE, OPT_DATABASE, OPT_HELP, OPT_TRACE, OPT_TRACE_DIR, OPT_TIMEOUT, OPT_EXEC, OPT_NO_STATUS, OPT_STATUS_FROM_JSON, OPT_VERSION };

CSimpleOpt::SOption g_rgOptions[] = {
	{ OPT_VERSION,         "-v",               SO_NONE },
	SO_END_OF_OPTIONS
};

void LogCommand(std::string line, UID randomID, std::string errMsg) {
	printf("%s\n", errMsg.c_str());
	//TraceEvent(SevInfo, "CLICommandLog", randomID).detail("Command", printable(StringRef(line))).detail("Error", printable(StringRef(errMsg)));
}

struct CLIOptions {
	std::string program_name;
	int exit_code;

	std::string commandLine;

	bool trace;
	std::string traceDir;
	int exit_timeout;
	Optional<std::string> exec;


	CLIOptions( int argc, char* argv[] )
		: trace(false),
		 exit_code(-1)
	{
		program_name = argv[0];
		for (int a = 0; a<argc; a++) {
			if (a) commandLine += ' ';
			commandLine += argv[a];
		}

		CSimpleOpt args(argc, argv, g_rgOptions);

		while (args.Next()) {
			int ec = processArg(args);
			if (ec != -1) {
				exit_code = ec;
				return;
			}
		}
		if (exit_timeout && !exec.present()) {
			fprintf(stderr, "ERROR: --timeout may only be specified with --exec\n");
			exit_code = 1;
			return;
		}
	}

	int processArg(CSimpleOpt& args) {
		if (args.LastError() != SO_SUCCESS) {
			return 1;
		}

		switch (args.OptionId()) {

			case OPT_TRACE:
				trace = true;
				break;
			case OPT_TRACE_DIR:
				traceDir = args.OptionArg();
				break;
			case OPT_TIMEOUT: {
				char *endptr;
				exit_timeout = strtoul((char*)args.OptionArg(), &endptr, 10);
				if (*endptr != '\0') {
					fprintf(stderr, "ERROR: invalid timeout %s\n", args.OptionArg());
					return 1;
				}
				break;
			}
			case OPT_EXEC:
				exec = args.OptionArg();
				break;


		}
		return -1;
	}
};

struct CommandHelp {
	std::string usage;
	std::string short_desc;
	std::string long_desc;
	CommandHelp() {}
	CommandHelp(const char* u, const char* s, const char* l) : usage(u), short_desc(s), long_desc(l) {}
};

std::map<std::string, CommandHelp> helpMap;
std::set<std::string> hiddenCommands;


void initHelp() {
	helpMap["begin"] = CommandHelp(
		"begin",
		"begin a new transaction",
		"By default, the fdbcli operates in autocommit mode. All operations are performed in their own transaction, and are automatically committed for you. By explicitly beginning a transaction, successive operations are all performed as part of a single transaction.\n\nTo commit the transaction, use the commit command. To discard the transaction, use the reset command.");


}

static bool tokencmp(StringRef token, const char *command)
{
	if (token.size() != strlen(command))
		return false;

	return !memcmp(token.begin(), command, token.size());
}

static std::vector<std::vector<StringRef>> parseLine(std::string& line, bool& err, bool& partial)
{
	err = false;
	partial = false;

	bool quoted = false;
	std::vector<StringRef> buf;
	std::vector<std::vector<StringRef>> ret;

	size_t i = line.find_first_not_of(' ');
	size_t offset = i;

	bool forcetoken = false;

	while (i <= line.length()) {
		switch (line[i]) {
			case ';':
				if (!quoted) {
					if (i > offset || (forcetoken && i == offset))
						buf.push_back(StringRef((uint8_t*)(line.data() + offset), i - offset));
					ret.push_back(std::move(buf));
					offset = i = line.find_first_not_of(' ', i+1);
					forcetoken = false;
				} else
					i++;
				break;
			case '"':
				quoted = !quoted;
				line.erase(i, 1);
				forcetoken = true;
				break;
			case ' ':
				if (!quoted) {
					if (i > offset || (forcetoken && i == offset))
						buf.push_back(StringRef((uint8_t*)(line.data() + offset), i - offset));
					offset = i = line.find_first_not_of(' ', i);
					forcetoken = false;
				} else
					i++;
				break;
			case '\\':
				if (i + 2 > line.length()) {
					err = true;
					ret.push_back(std::move(buf));
					return ret;
				}
				switch (line[i+1]) {
					char ent, save;
					case '"':
					case '\\':
					case ' ':
					case ';':
						line.erase(i, 1);
						break;
					case 'x':
						if (i + 4 > line.length()) {
							err = true;
							ret.push_back(std::move(buf));
							return ret;
						}
						char *pEnd;
						save = line[i + 4];
						line[i + 4] = 0;
						ent = char(strtoul(line.data() + i + 2, &pEnd, 16));
						if (*pEnd) {
							err = true;
							ret.push_back(std::move(buf));
							return ret;
						}
						line[i + 4] = save;
						line.replace(i, 4, 1, ent);
						break;
					default:
						err = true;
						ret.push_back(std::move(buf));
						return ret;
				}
			default:
				i++;
		}
	}

	i -= 1;
	if (i > offset || (forcetoken && i == offset))
		buf.push_back(StringRef((uint8_t*)(line.data() + offset), i - offset));

	ret.push_back(std::move(buf));

	if (quoted)
		partial = true;

	return ret;
}
std::string new_completion(const char *base, const char *name) {
	return format("%s%s ", base, name);
}

void comp_generator(const char* text, bool help, std::vector<std::string>& lc) {
	std::map<std::string, CommandHelp>::const_iterator iter;
	int len = strlen(text);

	const char* helpExtra[] = {"escaping", "options", NULL};

	const char** he = helpExtra;

	for (auto iter = helpMap.begin(); iter != helpMap.end(); ++iter) {
		const char* name = (*iter).first.c_str();
		if (!strncmp(name, text, len)) {
			lc.push_back( new_completion(help ? "help " : "", name) );
		}
	}

	if (help) {
		while (*he) {
			const char* name = *he;
			he++;
			if (!strncmp(name, text, len))
				lc.push_back( new_completion("help ", name) );
		}
	}
}

void cmd_generator(const char* text, std::vector<std::string>& lc) {
	comp_generator(text, false, lc);
}

void help_generator(const char* text, std::vector<std::string>& lc) {
	comp_generator(text, true, lc);
}

void complete_cmd(std::string const& text, std::vector<std::string>& lc) {
	bool err, partial;
	std::string whole_line = text;
	auto parsed = parseLine(whole_line, err, partial);
	if (err || partial) //If there was an error, or we are partially through a quoted sequence
		return;

	auto tokens = parsed.back();
	int count = tokens.size();

	// for(int i = 0; i < count; i++) {
	// 	printf("Token (%d): `%s'\n", i, tokens[i].toString().c_str());
	// }

	std::string ntext = "";
	std::string base_input = text;

	// If there is a token and the input does not end in a space
	if (count && text.size() > 0 && text[text.size() - 1] != ' ') {
		count--; //Ignore the last token for purposes of later code
		ntext = tokens.back().toString();
		base_input = whole_line.substr(0, whole_line.rfind(ntext));
	}

	// printf("final text (%d tokens): `%s' & `%s'\n", count, base_input.c_str(), ntext.c_str());

	if (!count) {
		cmd_generator(ntext.c_str(), lc);
		return;
	}

	if (tokencmp(tokens[0], "help") && count == 1) {
		help_generator(ntext.c_str(), lc);
		return;
	}
}



ACTOR Future<Void> timeWarning( double when, const char* msg ) {
	wait( delay(when) );
	fputs( msg, stderr );

	return Void();
}

ACTOR template <class T>
Future<T> stopNetworkAfter( Future<T> what ) {
	try {
		T t = wait(what);
		g_network->stop();
		return t;
	} catch (...) {
		g_network->stop();
		throw;
	}
}

ACTOR Future<int> cli(CLIOptions opt, LineNoise* plinenoise) {

	state LineNoise& linenoise = *plinenoise;
	state bool is_error = false;

	if (!opt.exec.present()) {
			printf("Welcome to flowcli. For help, type `help'.\n");
	}

	state Future<Void> warn;

	loop {
		if (warn.isValid())	warn.cancel();

		state std::string line;

		if (opt.exec.present()) {
			line = opt.exec.get();
		} else {
			Optional<std::string> rawline = wait( linenoise.read("flow> ") );

			if (!rawline.present()) {
				printf("\n");
				return 0;
			}
			line = rawline.get();

			if (!line.size())	continue;

			//warn = checkStatus( timeWarning(5.0, "\nWARNING: Long delay (Ctrl-C to interrupt)\n"), ccf );

			try {

				bool malformed, partial;
				state std::vector<std::vector<StringRef>> parsed = parseLine(line, malformed, partial);

				if (malformed || partial) {
					if (parsed.size() > 0) {
						// Denote via a special token that the command was a parse failure.
						auto& last_command = parsed.back();
						last_command.insert(last_command.begin(), StringRef((const uint8_t*)"parse_error", strlen("parse_error")));
					}
				}

				state bool multi = parsed.size() > 1;
				is_error = false;

				state std::vector<std::vector<StringRef>>::iterator iter;
				for (iter = parsed.begin(); iter != parsed.end(); ++iter) {
					state std::vector<StringRef> tokens = *iter;
					if (is_error) {
						printf("WARNING: the previous command failed, the remaining commands will not be executed.\n");
						break;
					}
					if (!tokens.size())
						continue;
					if (tokencmp(tokens[0], "exit") || tokencmp(tokens[0], "quit")) {
						return 0;
					}
				}
			} catch (Error& e) {
				if(e.code() != error_code_actor_cancelled)
					printf("ERROR: %s (%d)\n", e.what(), e.code());
				is_error = true;
			}
		}
	}
}


ACTOR Future<int> runCli(CLIOptions opt) {
	state LineNoise linenoise(
		[](std::string const& line, std::vector<std::string>& completions) {
			complete_cmd(line, completions);
		},
		[](std::string const& line)->LineNoise::Hint {
			return LineNoise::Hint();
		},
		1000,
		false);

	state std::string historyFilename;
	try {
		historyFilename = joinPath(getUserHomeDirectory(), ".fdbcli_history");
		linenoise.historyLoad(historyFilename);
	}
	catch(Error &e) {
		TraceEvent(SevWarnAlways, "ErrorLoadingCliHistory").error(e).detail("Filename", historyFilename.empty() ? "<unknown>" : historyFilename).GetLastError();
	}

	state int result = wait(cli(opt, &linenoise));

	if(!historyFilename.empty()) {
		try {
			linenoise.historySave(historyFilename);
		}
		catch(Error &e) {
			TraceEvent(SevWarnAlways, "ErrorSavingCliHistory").error(e).detail("Filename", historyFilename).GetLastError();
		}
	}

	return result;
}

ACTOR Future<Void> timeExit(double duration) {
	wait(delay(duration));
	fprintf(stderr, "Specified timeout reached -- exiting...\n");
	return Void();
}

int main(int argc, char **argv) {
	platformInit();
	initSignalSafeUnwind();
	Error::init();
	std::set_new_handler( &platform::outOfMemory );
	uint64_t memLimit = 8LL << 30;
	setMemoryQuota( memLimit );

	registerCrashHandler();

	initHelp();

	CLIOptions opt(argc, argv);

	try {
		setupNetwork();
		Future<int> cliFuture = runCli(opt);
		Future<Void> timeoutFuture = opt.exit_timeout ? timeExit(opt.exit_timeout) : Never();
		auto f = stopNetworkAfter( success(cliFuture) || timeoutFuture );
		runNetwork();

		if(cliFuture.isReady()) {
			return cliFuture.get();
		}
		else {
			return 1;
		}
	} catch (Error& e) {
			printf("ERROR: %s (%d)\n", e.what(), e.code());
			return 1;
		}
}
