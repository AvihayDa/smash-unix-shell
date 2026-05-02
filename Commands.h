// Ver: 10-4-2025
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <map>
#include <set>
#include <regex>
#include <limits.h>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
    // TODO: Add your data members
private:
    char* cmd_line = NULL;
    pid_t jobPid;
    char * alias_cmd_line = NULL;

public:
    Command(const char* cmd_line);
    
    virtual ~Command();

    virtual void execute() = 0;

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed - 1)

    const char* getCmdLine(); 

    void setAliasCmdLine(const char* cmd_line);

    const char* getAliasCmdLine();

    pid_t getPid();

    void setPid(pid_t pid);

    bool isComplexCommand(const char* cmd_line);   

    bool isFinished();

    // long long getDiskUsageKB(const char* path);
    long long getDiskUsageBytes(const char* path); // testing

};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char* cmd_line);

    virtual ~BuiltInCommand() {
    }
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char* cmd_line);

    virtual ~ExternalCommand() {
    }

    void execute() override;
};

class RedirectionCommand : public Command {
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char* cmd_line);

    virtual ~RedirectionCommand() {
    }

    void execute() override;
};

class PipeCommand : public Command {
    Command* lcmd;
    Command* rcmd;
    int pipeType = 1; // if the command is "cmd1 | cmd2"
public:
    PipeCommand(const char* cmd_line);

    virtual ~PipeCommand(); // THEY WROTE AN EMPTE D'TOR

    void execute() override;
};

class DiskUsageCommand : public Command {
public:
    DiskUsageCommand(const char* cmd_line);

    virtual ~DiskUsageCommand() {
    }

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char* cmd_line);

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    // TODO: Add your data members public:
    ChangeDirCommand(const char* cmd_line, char** plastPwd);

    virtual ~ChangeDirCommand() {}

    void execute() override;

    char** plastPwd = NULL;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char* cmd_line);

    virtual ~GetCurrDirCommand() {
    }

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char* cmd_line);

    virtual ~ShowPidCommand() {
    }

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
    // TODO: Add your data members public:         // WHY IN PUBLIC ??
public:
    JobsList* jobs;

    QuitCommand(const char* cmd_line, JobsList* jobs);

    virtual ~QuitCommand() {
    }

    void execute() override;
};

class JobsList {
public:
    class JobEntry {
    // TODO: Add your data members
    public:
        int jobId;
        bool isStopped = false;
        Command* cmd;

        JobEntry(int jobId, Command* cmd);

    };

    // TODO: Add your data members
    std::vector<JobEntry> job_entries;

public: // public again? 
    JobsList() = default; // Default constructor

    ~JobsList() = default; // Default destructor

    void addJob(Command* cmd, bool isStopped = false);  // DONE

    void printJobsList();  // DONE

    void killAllJobs();  // DONE

    void removeFinishedJobs();  // DONE

    JobEntry* getJobById(int jobId);  // DONE

    void removeJobById(int jobId);  // DONE

    JobEntry* getLastJob(int* lastJobId);

    JobEntry* getLastStoppedJob(int* jobId);


    // TODO: Add extra methods or modify exisitng ones as needed

    void sortJobsById();

    int getNextId();
};

class JobsCommand : public BuiltInCommand {
private:
    // TODO: Add your data members
    JobsList* jobs;

public:
    JobsCommand(const char* cmd_line, JobsList* jobs);

    virtual ~JobsCommand() {
    }

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    // TODO: Add your data members
private:
    JobsList* jobs;
   
public:
    KillCommand(const char* cmd_line, JobsList* jobs);

    virtual ~KillCommand() {
    }

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    // TODO: Add your data members
private:
    JobsList* jobs;

public:
    ForegroundCommand(const char* cmd_line, JobsList* jobs);

    virtual ~ForegroundCommand() {
    }

    void execute() override;
};

class AliasCommand : public BuiltInCommand {
public:
    static const std::regex pattern; // Declare as a static member

    AliasCommand(const char *cmd_line, std::map<std::string, std::string>* existingAlias, std::set<std::string>* resKeywords, std::vector<std::string>* aliasK);

    virtual ~AliasCommand() {
    }

    void execute() override;

    std::map<std::string, std::string>* alias = nullptr;
    std::set<std::string>* reservedKeywords = nullptr;
    std::vector<std::string>* aliasKeys = nullptr; // for keeping track of the order of aliases
    bool okToExe = true;
    bool specialAlias = false;
    std::string cmd;
    std::string cmd_name;



};

class UnAliasCommand : public BuiltInCommand {
private:
    std::map<std::string, std::string>* existingAlias = nullptr;
    std::vector<std::string>* aliasKeys = nullptr; // for keeping track of the order of aliases

public:
    UnAliasCommand(const char *cmd_line, std::map<std::string, std::string>* alias, std::vector<std::string>* aliasK);

    virtual ~UnAliasCommand() {
    }

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand {
public:
    UnSetEnvCommand(const char* cmd_line);

    virtual ~UnSetEnvCommand() {
    }

    void execute() override;
};

class SysInfoCommand : public BuiltInCommand {
public:
    SysInfoCommand(const char *cmd_line);

    virtual ~SysInfoCommand() {
    }

    void execute() override;
};


class SmallShell {
private:
    // TODO: Add your data members
    std::string promptName = "smash";
    char* lastDir = NULL;
    JobsList* jobList = nullptr;
    std::map<std::string, std::string> alias;
    std::vector<std::string> aliasKeys; // for keeping track of the order of aliases
    std::set<std::string> reservedKeywords = {"chprompt", "showpid", "pwd", "cd", "jobs", "fg", "quit", "kill", "alias", "unalias", "unsetenv", "sysinfo", "du", "whoami", "netinfo"};

    pid_t currentForegroundPid = -1;

    SmallShell();

public:
    Command* CreateCommand(const char* cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell(); // don't forget free() the lastDir ptr (if not null ofcourse)

    void executeCommand(const char* cmd_line);

    // TODO: add extra methods as needed

    void promptSetter(const std::string& newPrompt){
        promptName = newPrompt;
    }

    std::string getPrompt(){
        return promptName;
    }
    
    pid_t getForegPid(){
        return currentForegroundPid;
    }

    void setForegPid(pid_t pid){
        currentForegroundPid = pid;
    }

};

#endif //SMASH_COMMAND_H_
