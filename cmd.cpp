/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include <iostream>
#include "config.h"
#include <folly/String.h>
#include <map>
#include <set>
#include <fstream>
#include <tclap/CmdLine.h>
#include <stdlib.h>
#include "main.h"

void showVersion()
{
	std::cout << "HS5 v" << PACKAGE_VERSION << std::endl;
	std::cout << "Copyright (C) 2025 Martin Raiber" << std::endl;
	std::cout << "This is free software; see the source for copying conditions. There is NO"<< std::endl;
	std::cout << "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE."<< std::endl;
}

std::string_view unquoteValue(std::string_view val)
{
	val = folly::trimWhitespace(val);
    if(val.empty())
        return val;

	if(val[0]=='"')
	{
		size_t last_pos = val.find_last_of('"');
		if(last_pos!=0)
		{
			val=val.substr(1, last_pos-1);
		}
	}
	else if(val[0]=='\'')
	{
		size_t last_pos = val.find_last_of('\'');
		if(last_pos!=0)
		{
			val=val.substr(1, last_pos-1);
		}
	}
	return val;
}

std::map<std::string_view, std::string_view> readSettings(const std::string_view& data)
{
    std::map<std::string_view, std::string_view> settings;
    std::string_view::size_type pos = 0;
    while(pos!=std::string_view::npos)
    {
        auto nextPos = data.find('\n', pos);
        const auto lastLine = nextPos == std::string_view::npos;
        if(lastLine)
        {
            nextPos = data.size()+1;
        }
        const auto line = data.substr(pos, nextPos-pos);
        if(line.empty() || line[0]=='#')
        {
            pos = lastLine ? std::string_view::npos : nextPos+1;
            continue;
        }
        const auto eqPos = line.find('=');
        if(eqPos!=std::string_view::npos)
        {
            const auto key = line.substr(0, eqPos);
            const auto val = line.substr(eqPos+1);
            settings[key] = unquoteValue(val);
        }
        pos = lastLine ? std::string_view::npos : nextPos+1;
    }
    return settings;
}

std::string readFile(const std::string& fn)
{
    std::ifstream file(fn);
    if(!file)
    {
        throw std::runtime_error("Cannot open file "+fn);
    }
    file.exceptions(std::ifstream::badbit);
    std::string data;
    file.seekg(0, std::ios::end);
    data.resize(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(&data[0], data.size());
    return data;
}

struct ConfigResult
{
    std::vector<std::string> args;
    std::set<std::string> setArgs;
};

bool parseConfigBool(const std::string_view val)
{
    if(val=="true" || val=="yes" || val=="on" || val=="1")
        return true;
    return false;
}

std::string toFollyLoglevel(const std::string_view val)
{
    if(val=="debug")
        return "DBG0";
    if(val=="info")
        return "INFO";
    if(val=="warn")
        return "WARN";
    if(val=="error")
        return "ERR";
    return "INFO";
}

ConfigResult readConfigFile(const std::string& fn)
{
    ConfigResult res;
    const auto data = readFile(fn);
    const auto settings = readSettings(data);

    const std::map<std::string_view, std::string_view> configMapping = {
        {"HTTP_PORT", "http_port"},
        {"SERVER_URL", "server_url"},
        {"DATA_STORAGE_PATH", "data_path"},
        {"METADATA_STORAGE_PATH", "index_path"},
        {"SERVER_IP", "ip"},
        {"INIT_ROOT_SECRET", "init_root_password"}
    };

    for(const auto& [key, val]: settings)
    {
        if(key=="MANUAL_COMMIT")
        {
            if(parseConfigBool(val))
            {
                res.args.push_back("--manual_commit");
                res.setArgs.insert("--manual_commit");
            }
            continue;
        }
        if(key=="LOGLEVEL")
        {
            res.setArgs.insert("--logging");
            res.args.push_back("--logging");
            res.args.push_back(toFollyLoglevel(val));
            continue;
        }

        const auto it = configMapping.find(key);
        if(it!=configMapping.end())
        {
            res.args.push_back("--"+std::string(it->second));
            res.setArgs.insert("--"+std::string(it->second));
            res.args.push_back(std::string(val));
        }
    }
    return res;
}

int runRealMain(std::vector<std::string> args)
{
    char** argv = new char*[args.size()];
	for(size_t i=0;i<args.size();++i)
	{
		argv[i] = const_cast<char*>(args[i].c_str());
	}

	int rc = realMain(static_cast<int>(args.size()), argv);
	delete[] argv;
	return rc;
}

typedef int(*action_fun)(std::vector<std::string> args);

int actionRun(std::vector<std::string> args)
{
    TCLAP::CmdLine cmd("Run HS5", ' ', PACKAGE_VERSION);

    std::vector<std::string> loglevels;
	loglevels.push_back("debug");
	loglevels.push_back("info");
	loglevels.push_back("warn");
	loglevels.push_back("error");

    TCLAP::ValuesConstraint<std::string> loglevelsConstraint(loglevels);
	TCLAP::ValueArg<std::string> loglevelArg("v", "loglevel",
		"Specifies the log level (default info)",
		false, "info", &loglevelsConstraint, cmd);

    TCLAP::ValueArg<unsigned short> httpPortArg("p", "http-port",
		"Specifies the port where HS5 will listen for HTTP connections (default 80)",
		false, 80, "port number", cmd);

    TCLAP::ValueArg<std::string> configArg("c", "config",
		"Read configuration parameters from config file",
		false, "", "path", cmd);

    TCLAP::ValueArg<std::string> serverUrlArg("u", "server-url",
		"URL of the server",
		false, "https://example.com", "URL", cmd);

    TCLAP::ValueArg<std::string> storagePathArg("s", "storage-path",
		"Storage path (default current dir)",
		false, ".", "path", cmd);

    TCLAP::ValueArg<std::string> dataStoragePathArg("d", "data-storage-path",
		"Data file storage path (default current dir)",
		false, ".", "path", cmd);

    TCLAP::ValueArg<std::string> metadataStoragePathArg("m", "metadata-storage-path",
		"Metadata database storage path (default current dir)",
		false, ".", "path", cmd);

    TCLAP::ValueArg<std::string> ipArg("i", "ip",
		"Server IP/hostname to bind to (default 0.0.0.0)",
		false, "0.0.0.0", "ip", cmd);

    TCLAP::ValueArg<std::string> initRootSecretArg("", "init-root-secret",
		"Initial root access key secret",
		false, "", "string", cmd);

    TCLAP::SwitchArg manualCommitArg("", "manual-commit",
        "Manual commit mode (default false)", cmd);

    std::vector<std::string> realArgs;
	realArgs.push_back(args[0]);

    cmd.parse(args);

    std::set<std::string> alreadySetArgs;
    if(!configArg.getValue().empty())
	{
		const auto configRes = readConfigFile(configArg.getValue());
        args.insert(args.end(), configRes.args.begin(), configRes.args.end());
	}

    if(!alreadySetArgs.contains("--http_port"))
    {
        realArgs.push_back("--http_port");
        realArgs.push_back(std::to_string(httpPortArg.getValue()));
    }

    if(!alreadySetArgs.contains("--server_url"))
    {
        realArgs.push_back("--server_url");
        realArgs.push_back(serverUrlArg.getValue());
    }

    if(!alreadySetArgs.contains("--data_path"))
    {
        if( dataStoragePathArg.isSet()  || storagePathArg.isSet()) 
        {
            const auto dataPath = dataStoragePathArg.isSet() ? dataStoragePathArg.getValue() : storagePathArg.getValue();
            realArgs.push_back("--data_path");
            realArgs.push_back(dataStoragePathArg.getValue());
        }
        else if(auto dataPath = getenv("DATA_PATH"); dataPath)
        {
            realArgs.push_back("--data_path");
            realArgs.push_back(dataPath);
        }
        else if(auto dataPath = getenv("STORAGE_PATH"); dataPath)
        {
            realArgs.push_back("--data_path");
            realArgs.push_back(dataPath);
        }
    }

    if(!alreadySetArgs.contains("--index_path"))
    {
        if( metadataStoragePathArg.isSet()  || storagePathArg.isSet()) 
        {
            const auto dataPath = metadataStoragePathArg.isSet() ? metadataStoragePathArg.getValue() : storagePathArg.getValue();
            realArgs.push_back("--index_path");
            realArgs.push_back(metadataStoragePathArg.getValue());
        }
        else if(auto metadataPath = getenv("METADATA_PATH"); metadataPath)
        {
            realArgs.push_back("--index_path");
            realArgs.push_back(metadataPath);
        }
        else if(auto metadataPath = getenv("STORAGE_PATH"); metadataPath)
        {
            realArgs.push_back("--index_path");
            realArgs.push_back(metadataPath);
        }
    }

    if(!alreadySetArgs.contains("--ip"))
    {
        realArgs.push_back("--ip");
        realArgs.push_back(ipArg.getValue());
    }

    if(!alreadySetArgs.contains("--init_root_password"))
    {
        if(initRootSecretArg.isSet())
        {
            realArgs.push_back("--init_root_password");
            realArgs.push_back(initRootSecretArg.getValue());
        }
        else if(auto initRootSecret = getenv("INIT_ROOT_SECRET"); initRootSecret)
        {
            realArgs.push_back("--init_root_password");
            realArgs.push_back(initRootSecret);
        }
    }

    if(!alreadySetArgs.contains("--manual_commit")
        && manualCommitArg.getValue())
    {
        realArgs.push_back("--manual_commit");
    }

    if(!alreadySetArgs.contains("--logging"))
    {
        realArgs.push_back("--logging");
        realArgs.push_back(toFollyLoglevel(loglevelArg.getValue()));
    }

    return runRealMain(realArgs);
}

int actionHelp(std::vector<std::string> args)
{
	std::cout << std::endl;
	std::cout << "USAGE:" << std::endl;
	std::cout << std::endl;
	std::cout << "\t" << args[0] << " [--help] [--version] <command> [<args>]" << std::endl;
	std::cout << std::endl;
	std::cout << "Get specific command help with " << args[0] << " <command> --help" << std::endl;
	std::cout << std::endl;
	std::cout << "\t" << args[0] << " run" << std::endl;
	std::cout << "\t\t" "Run HS5" << std::endl;
    return 0;
}

int actionInternal(std::vector<std::string> args)
{
	return runRealMain(args);
}

const std::map<std::string, action_fun> cmdlineActions = {
        {"run", actionRun},
        {"help", actionHelp},
        {"internal", actionInternal}
    };

int main(int argc, char* argv[])
{
	if(argc==0)
	{
		std::cout << "Not enough arguments (zero arguments) -- no program name" << std::endl;
		return 1;
	}

    bool hasHelp=false;
	bool hasVersion=false;
    action_fun action = nullptr;
    std::string actionName;

    std::vector<std::string> args;
	args.push_back(argv[0]);

    for(int i=1;i<argc;++i)
	{
		const std::string arg = argv[i];

		if(arg=="--help" || arg=="-h")
		{
			hasHelp=true;
		}

		if(arg=="--version")
		{
			hasVersion=true;
		}

        if(!arg.empty() && arg[0]=='-')
		{
			args.push_back(arg);
			continue;
		}

        const auto actionIt = cmdlineActions.find(arg);
        if(actionIt != cmdlineActions.end())
        {
            if(action!=nullptr)
            {
                actionHelp(args);
				exit(1);
            }
            action = actionIt->second;
            actionName = arg;
            continue;
        }
        else
        {
            args.push_back(arg);
        }
    }

    if(action==nullptr)
    {
        if(hasHelp)
		{
			actionHelp(args);
			exit(1);
		}
        if(hasVersion)
		{
			showVersion();
			exit(1);
		}
        actionHelp(args);
		exit(1);
    }

    try
	{
		args[0]+=" "+actionName;
		int rc = action(args);
		return rc;
	}
	catch (TCLAP::ArgException &e)
	{
		std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
		return 1;
	}
}

