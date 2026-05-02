#include <unistd.h>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <cstdint>
#include <unistd.h>
#include <regex>

using namespace std;

extern char** __environ;

const std::string WHITESPACE = " \n\r\t\f\v";

const std::regex AliasCommand::pattern(R"(^alias [a-zA-Z0-9_]+='[^']*'$)");

#if 0
#define FUNC_ENTRY()  \
  std::cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  std::cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

//-----------------------------------Helper Functions-----------------------------------//

string cutFirstWord(const string& str) {
    size_t firstSpace = str.find(' ');
    if (firstSpace == std::string::npos) {
        return "";
    }
    return str.substr(firstSpace + 1);
}

/*
* Purpose: Removes leading whitespace from the string s.
* Returns the string starting from that index.
*/
string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

/*
* Purpose: Removes trailing (from the end) whitespace from the string s.
* Returns the string up to and including that character.
*/
string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

/*
* Purpose: Removes both leading and trailing whitespace.
* Returns the updated string striped on bothsides from whitespace.
*/
string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

/*
* Purpose: Splits a command line string into arguments and stores them in args.
* Function fills the given array of char* with the args, each bucket holds one argument
* Returns the number of arguments.
*/
int _parseCommandLine(const char* cmd_line, char** args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

/*
* Purpose: Checks if the command line ends with an & (meaning it should run in the background).
* Returns true if the given command should run in the background.
*/
bool _isBackgroundComamnd(const char* cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

/*
* Purpose: Removes the background & symbol from the end of the command line.
*/
void _removeBackgroundSign(char* cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}



//------------------------------- OUR TIME TO SHINE ----------------------------------//


//=========================== class Command ==========================================//

Command::Command(const char* cmd_line){
    this->cmd_line = new char[strlen(cmd_line) + 1];
    strcpy(this->cmd_line, cmd_line);
}

Command::~Command(){
    if(cmd_line != NULL){
        delete[] cmd_line;
    }
    if(alias_cmd_line != NULL){
        delete[] alias_cmd_line;
    }
}

const char* Command::getCmdLine(){
    return cmd_line;
}

void Command::setAliasCmdLine(const char* cmd_line){
    this->alias_cmd_line = new char[strlen(cmd_line) + 1];
    strcpy(this->alias_cmd_line, cmd_line);
}

const char* Command::getAliasCmdLine() {
    if (alias_cmd_line != NULL){
        return alias_cmd_line;
    }
    return cmd_line;
}

pid_t Command::getPid(){
    return jobPid;
}

void Command::setPid(pid_t pid){
    jobPid = pid;
}

bool Command::isComplexCommand(const char* cmd_line){
    return string(cmd_line).find("*") != string::npos || string(cmd_line).find("?") != string::npos;
}

bool Command::isFinished(){
    int status;
    pid_t result = waitpid(this->jobPid, &status, WNOHANG);
    if(result == 0){
        return false;
    }
    return true;
}



//=========================== class BuiltInCommand ==================================//

BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line) {}



//=========================== class ExternalCommand ==================================//

ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line) {}

void ExternalCommand::execute(){
    string cmd_s = string(getCmdLine());
    bool is_complex = isComplexCommand(getCmdLine());
    if(cmd_s.find("&") != string::npos){
        cmd_s.erase(cmd_s.find("&"), 1); // Remove the "&" sign if there is
    }

    if(cmd_s.find(">") != string::npos){
        cmd_s.erase(cmd_s.find(">"));
        cmd_s = _trim(cmd_s);
    }
    
    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(cmd_s.c_str(), args);
    if(is_complex == true){
        char* bash_args[] = {const_cast<char*>("/bin/bash"), const_cast<char*>("-c"), const_cast<char*>(cmd_s.c_str()), nullptr};
        if(execvp(bash_args[0], bash_args) == -1){
            perror("smash error: execvp failed");
            exit(EXIT_FAILURE);
        }
    }
    else if(is_complex == false){
        if(execvp(args[0], args) == -1){
            perror("smash error: execvp failed");
            exit(EXIT_FAILURE);
        }
    }
}



//=========================== class WhoAmICommand ==================================//

WhoAmICommand::WhoAmICommand(const char* cmd_line) : Command(cmd_line) {}

void WhoAmICommand::execute(){
    const char* env_path = "/proc/self/status";
    char buffer[8192];
    int fd = open(env_path, O_RDONLY);
    if(fd == -1){
        perror("smash error: open failed");
        return;
    }
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if(bytes_read == -1){
        perror("smash error: read failed");
        close(fd);
        return;
    }
    close(fd);
    buffer[bytes_read] = '\0';
    char* uid_line = strstr(buffer, "Uid:");
    if(!uid_line){
        cerr << "smash error: Uid line not found in /proc/self/status" << endl;
        return;
    }
    int uid_target = 0 ;
    if(sscanf(uid_line, "Uid:%d", &uid_target) != 1){
        cerr << "smash error: failed to parse Uid line" << endl;
        return;
    }
    memset(buffer, 0, sizeof(buffer)); // clear buffer for next read
    fd = open("/etc/passwd", O_RDONLY);
    if(fd == -1){
        perror("smash error: open failed");
        return;
    }
    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if(bytes_read == -1){
        perror("smash error: read failed");
        close(fd);
        return;
    }
    close(fd);
    buffer[bytes_read] = '\0';

    char* p = buffer;
    while (*p) {
        char *end = strchr(p, '\n');
        if (end) *end = '\0';

        int ok = 1;

        char *c1 = strchr(p, ':');
        if(!c1) ok = 0;

        char *c2 = ok ? strchr(c1 + 1, ':') : NULL;
        if(!c2) ok = 0;
        
        char *c3 = ok ? strchr(c2 + 1, ':') : NULL;
        if(!c3) ok = 0;

        int uid = -1;
        if(ok){
            char* uid_start = c2 + 1;
            uid = (int)strtol(uid_start, NULL, 10);
            if(uid != uid_target){
                ok = 0;
            }
        }

        if(ok){
            char *c4 = strchr(c3 + 1, ':');   // לפני comment
            char *c5 = c4 ? strchr(c4 + 1, ':') : NULL; // לפני home
            char *c6 = c5 ? strchr(c5 + 1, ':') : NULL; // לפני shell
            if(!c4 || !c5 || !c6) ok = 0;
            else{
                *c1 = '\0';
                *c2 = '\0';
                *c3 = '\0';
                *c4 = '\0';
                *c5 = '\0';
                *c6 = '\0';

                char *username = p;
                char *gid_str  = c3 + 1;
                char *home     = c5 + 1;
                printf("%s\n", username);
                printf("%d\n", uid);
                printf("%s\n", gid_str);
                printf("%s\n", home);
                memset(buffer, 0, sizeof(buffer)); // clear buffer
                break;
            }
        }
        if (!end) break;
        p = end + 1;
    }
    
}




//=========================== class DiskUsageCommand ==================================//

struct linux_dirent {
    long d_ino;         
    off_t d_off;        
    unsigned short d_reclen; 
    char d_name[];      
};

void DiskUsageCommand::execute(){
    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(getCmdLine(), args);
    if(numArgs > 2){
        std::cerr << "smash error: du: too many arguments" << std::endl;
        return;
    }
    const char* targetPath = (numArgs == 2) ? args[1] : nullptr;
    char curPath[PATH_MAX];
    if(targetPath == nullptr){     // No path provided (so use current working directory) 
        if(getcwd(curPath, sizeof(curPath)) == nullptr){
            perror("smash error: getcwd failed");
            return;
        }
        targetPath = curPath;
    }
    long long bytes = getDiskUsageBytes(targetPath);
    long long kb = (bytes + 1023) / 1024;   // ceil
    cout << "Total disk usage: " << kb << " KB" << endl;
}

long long Command::getDiskUsageBytes(const char* path) {
    struct stat st;
    if (lstat(path, &st) == -1) {
        perror("smash error: lstat failed");
        return 0;
    }

    if (S_ISLNK(st.st_mode)) {
        return 0;
    }

    long long total = (long long)st.st_blocks * 512;

    if (!S_ISDIR(st.st_mode)) {
        return total;
    }

    int fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd == -1) {
        perror("smash error: open failed");
        return total;
    }

    char buf[8192];

    while (true) {
        int nread = syscall(SYS_getdents, fd, buf, sizeof(buf));
        if (nread == -1) {
            perror("smash error: getdents failed");
            close(fd);
            return total;
        }
        if (nread == 0)
            break;

        int offset = 0;
        while (offset < nread) {
            linux_dirent* d = (linux_dirent*)(buf + offset);
            offset += d->d_reclen;

            const char* name = d->d_name;
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;

            std::string fullPath = std::string(path) + "/" + name;

            total += getDiskUsageBytes(fullPath.c_str());
        }
    }

    close(fd);
    return total;
}


DiskUsageCommand::DiskUsageCommand(const char* cmd_line) : Command(cmd_line) {}


//=========================== class ShowPidCommand ==================================//

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute(){
    pid_t pid = getpid();
    if(pid == -1){
        perror("smash error: getpid failed");
    }
    cout << "smash pid is " << pid << "\n";
}


//=========================== class GetCurrDirCommand =============================//

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute(){
    
    char *path = getcwd(NULL, 0);
    if (path != NULL) {
        printf("%s\n", path);
        free(path);
    } 
    else {
      perror("smash error: getcwd failed");
    }
}

//=========================== class ChangeDirCommand =============================//

ChangeDirCommand::ChangeDirCommand(const char* cmd_line, char **plastPwd) : BuiltInCommand(cmd_line), plastPwd(plastPwd) {}

void ChangeDirCommand::execute(){
    
    std::string cmd_s = _trim(string(getCmdLine())); // cast to String + trimming spaces
    size_t first_lt = cmd_s.find(">");
    if (first_lt != string::npos){
        cmd_s.erase(first_lt); // Remove everything starting from the first '<' to the end of the string
        cmd_s = _trim(cmd_s);
    }

    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(cmd_s.c_str(), args);
    if(numArgs > 2){ // in term of the command name counted as argument
        std::cerr << "smash error: cd: too many arguments" << std::endl;
        return;
    }
    else if(numArgs == 1) return;
    
    string newDir;
    string arg1 = _trim(string(args[1])); // trimming spaces around the argument
    
    if(arg1 == "-"){
        if(*plastPwd){ // check if lastPwd(saved by smash) is not null
            newDir = string(*plastPwd); // change to lastPWD
        }
        else{ // lastPWD is null - lastPWD was never set before
            std::cerr << "smash error: cd: OLDPWD not set" << endl;
            return;
        }
    }
    else newDir = string(args[1]); // normal case - change to the given directory. we can assume its valid path
    if(newDir == "") return; // if newDir is empty string - do nothing

    char* currDir = getcwd(NULL, 0); // save current directory before changing it

    if(chdir(newDir.c_str()) != 0){ // change directory, if fails - print error and return
        perror("smash error: chdir failed");
        if(currDir != NULL){
            free(currDir);
        }
        return;
    }

    if(*plastPwd) free(*plastPwd); // free the old lastPWD if not null
    *plastPwd = currDir; // update lastPWD to the previous current directory
}

//=========================== class AliasCommand =============================//

AliasCommand::AliasCommand(const char *cmd_line, std::map<std::string, std::string>* existingAlias, std::set<std::string>* resKeywords, std::vector<std::string>* aliasK) : BuiltInCommand(cmd_line), alias(existingAlias), reservedKeywords(resKeywords), aliasKeys(aliasK) {
    std::string cmd_s = _trim(string(cmd_line)); // cast to String + trimming spaces
    if(cmd_s == "alias"){
        specialAlias = true;
        cmd = "";
        cmd_name = "";
        return;
    }
    if(cmd_s.find("&") != string::npos){
        if(cmd_s.find("&") > cmd_s.rfind("'")){
            cmd_s.erase(cmd_s.find("&"), 1); // Remove the "&" sign if there is
        }
    }

    if(cmd_s.find(">") != string::npos){
        cmd_s.erase(cmd_s.find(">"));
        cmd_s = _trim(cmd_s);
    }

    if(!(std::regex_match(cmd_s, pattern))){
        std::cerr << "smash error: alias: invalid alias format" << endl;
        this->okToExe = false;
        return;
    }
    cmd_s = cutFirstWord(cmd_s); //remove the prefix "alias "
    std::string cmd_name = cmd_s.substr(0, cmd_s.find("="));


    if(reservedKeywords->find(cmd_name) != reservedKeywords->end() || alias->count(cmd_name) > 0){
        std::cerr << "smash error: alias: " + cmd_name + " already exists or is a reserved command" << endl;
        this->okToExe = false;
        return;
    }
    std::string alias_cmd = cmd_s.substr(cmd_s.find("'") + 1, (cmd_s.rfind("'") - cmd_s.find("'") - 1));
    this->cmd = alias_cmd;
    this->cmd_name = cmd_name;

}

void AliasCommand::execute(){
    if(!(this->okToExe)) return;
    if(specialAlias){
        for(auto e : *this->aliasKeys){
            std::cout << e << "='" << (*this->alias)[e] << "'" << endl;
        }
        return;
    }
    this->alias->insert({this->cmd_name, this->cmd});
    this->aliasKeys->push_back(this->cmd_name);
}


//=========================== class UnAliasCommand ===========================//

UnAliasCommand::UnAliasCommand(const char *cmd_line, std::map<std::string, std::string>* alias, std::vector<std::string>* aliasK) : BuiltInCommand(cmd_line), existingAlias(alias), aliasKeys(aliasK) {}

void UnAliasCommand::execute(){
    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(getCmdLine(), args);
    if(numArgs == 1){ // in term of the command name counted as argument
        cerr << "smash error: unalias: not enough arguments" << endl;
        return;
    }
    for(int i = 1 ; i < numArgs ; ++i){
        if(this->existingAlias->count(args[i]) == 0){
            cerr << "smash error: unalias: " << args[i] << " alias does not exist" << endl;
            return;
        }
        this->existingAlias->erase(args[i]);
        auto it = std::find(aliasKeys->begin(), aliasKeys->end(), args[i]);
        aliasKeys->erase(it);
    }    
}




//=========================== class JobsCommand =============================//

JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand::BuiltInCommand(cmd_line) , jobs(jobs) {}

void JobsCommand::execute(){ 
    jobs->printJobsList();
}


//=========================== class ForegroundCommand =============================//

ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand::BuiltInCommand(cmd_line) , jobs(jobs) {}

void ForegroundCommand::execute(){
    jobs->removeFinishedJobs();
    char* args[COMMAND_MAX_ARGS];
    int jobIdToUse = -1;
    int numArgs = _parseCommandLine(getCmdLine(), args);
    bool isValidId = true;
    if(numArgs > 1){
        for(int i = 0 ; i < strlen(args[1]) ; ++i){
            if(args[1][i] < 48 || args[1][i] > 57){
                isValidId = false;
                break;
            }
        }
    }
    if(numArgs > 2 || !isValidId){    
        cerr << "smash error: fg: invalid arguments" << endl;
        return;
    }
    // Recieved argument for id
    if(numArgs == 2){
        try{
            jobIdToUse = stoi(string(args[1])); 
        } catch (const invalid_argument& e) {
            cerr << "smash error: fg: invalid arguments" << endl;
            return;
        }
        JobsList::JobEntry* job = jobs->getJobById(jobIdToUse);
        // Recieved valid id but job dosn't exist
        if(job == nullptr){  
            cerr << "smash error: fg: job-id " << jobIdToUse << " does not exist" << endl;
            return;
        }
        // Recieved valid id and job exists
        else if(job != nullptr){
            std::cout << job->cmd->getCmdLine() << " " << job->cmd->getPid() << std::endl ;
            pid_t pid = job->cmd->getPid();
            SmallShell::getInstance().setForegPid(pid);
            job->isStopped = false;
            int status;
            if(waitpid(job->cmd->getPid(), &status, WUNTRACED) == -1){
                perror("smash error: waitpid failed");
                return;
            }
            if(WIFSTOPPED(status)){
                job->isStopped = true;
            }
            else{
                jobs->removeJobById(jobIdToUse);
            }
            SmallShell::getInstance().setForegPid(-1);
        }
    }
    // No id was given
    if(numArgs == 1){  
        if(jobs->job_entries.empty()){
            cerr << "smash error: fg: jobs list is empty" << endl;
            return;
        }
        jobIdToUse = jobs->getNextId() - 1;
        JobsList::JobEntry* job = jobs->getJobById(jobIdToUse);
        jobs->removeJobById(jobIdToUse);
        std::cout << job->cmd->getCmdLine() << " " << job->cmd->getPid() << std::endl ;
        pid_t pid = job->cmd->getPid();
        SmallShell::getInstance().setForegPid(pid);
        job->isStopped = false;
        int status;
        if(waitpid(job->cmd->getPid(), &status, WUNTRACED) == -1){
            perror("smash error: waitpid failed");
            return;
        }
        if(WIFSTOPPED(status)){
            job->isStopped = true;
        }
        else{
            jobs->removeJobById(jobIdToUse);
        }
        SmallShell::getInstance().setForegPid(-1);
        return;
    }
}




//=========================== class QuitCommand =============================//

QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line) , jobs(jobs) {}

void QuitCommand::execute(){
    string cmd_line = _trim(getCmdLine());
    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(getCmdLine(), args);
    if(numArgs >= 2 && strcmp(args[1], "kill") == 0){
        jobs->removeFinishedJobs();
        jobs->killAllJobs();
    }
    exit(0);
}
    


//=========================== class KillCommand =============================//

KillCommand::KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line) , jobs(jobs) {}

void KillCommand::execute(){
    string cmd_line = _trim(getCmdLine());
    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(getCmdLine(), args);
    jobs->removeFinishedJobs();

    bool isValidId = true;
    if(numArgs > 2){
        for(int i = 0 ; i < strlen(args[2]) ; ++i){
            if(args[2][i] < 48 || args[2][i] > 57){
                isValidId = false;
                break;
            }
        }
    }




    if(numArgs != 3 || args[1][0] != '-' || !isValidId){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    int jobId;      
    int sigNum;    
    try{
        sigNum = stoi(string(args[1] + 1));
        jobId = stoi(string(args[2]));
    } catch(const exception& e){
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    JobsList::JobEntry* job = jobs->getJobById(jobId);
    if(job == nullptr){
        cerr << "smash error: kill: job-id " << jobId << " does not exist" << endl;
        return;
    }
    std::cout << "signal number " << sigNum << " was sent to pid " << job->cmd->getPid() << endl;

    if(kill(job->cmd->getPid(), sigNum) == -1){
        perror("smash error: kill failed");
        return;
    }
    //std::cout << "signal number " << sigNum << " was sent to pid " << job->cmd->getPid() << endl;
    return;
}

//============================= class UnSetEnvCommand ==================================//

 bool env_var_exists_procfs(const std::string& var){
    int fd = open("/proc/self/environ", O_RDONLY);
    if(fd < 0){
        perror("smash error: open failed");
        return false;
    }
    char buffer[8192];  // should be large enough for most environments
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
    close(fd);
    if(bytes_read <= 0){
        return false;
    }
    size_t i = 0;
    while(i < (size_t)bytes_read){
        std::string entry = std::string(&buffer[i]);
        size_t eq_pos = entry.find('=');
        if(eq_pos != std::string::npos) {
            std::string key = entry.substr(0, eq_pos);
            if(key == var){
                return true;
            }
        }
        i += entry.size() + 1; // entries are NUL-separated
    }
    return false;
}


bool remove_env_var(const std::string& var) {
    size_t len = var.length();
    for (char** env = __environ; *env != nullptr; ++env) {
        if (strncmp(*env, var.c_str(), len) == 0 && (*env)[len] == '=') {
            // Found the variable, remove by shifting all elements after it
            for (char** next = env; *next != nullptr; ++next) {
                *next = *(next + 1);
            }
            return true;
        }
    }
    return false;  // should never reach if you checked via /proc first
}

UnSetEnvCommand::UnSetEnvCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void UnSetEnvCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int numArgs = _parseCommandLine(getCmdLine(), args);
    if(numArgs == 1){ // in term of the command name counted as argument
        cerr << "smash error: unsetenv: not enough arguments" << endl;
        return;
    }
    for(int i = 1 ; i < numArgs ; ++i){
        if( !(env_var_exists_procfs(string(args[i]))) ){
            cerr << "smash error: unsetenv: " << args[i] << " does not exist" << endl;
            return;
        }
        remove_env_var(string(args[i]));
    }
}


//============================= class SysInfoCommand ==================================//

SysInfoCommand::SysInfoCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void SysInfoCommand::execute(){
    int fd = open("/proc/sys/kernel/ostype", O_RDONLY);
    if(fd == -1){
        perror("smash error: open failed");
        return;
    }
    char buffer[128];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if(bytes_read == -1){
        perror("smash error: read failed");
        close(fd);
        return;
    }
    buffer[bytes_read] = '\0'; // Null-terminate the buffer
    if(close(fd) == -1){
        perror("smash error: close failed");
        return;
    }
    char* i = buffer;
    while(*i != '\0'){
        if(*i == '\n'){
            *i = '\0';
            break;
        }
        ++i;
    }
    string sys_str = buffer; 
    memset(buffer, 0, sizeof(buffer)); // clear buffer for next read

    fd = open("/proc/sys/kernel/hostname", O_RDONLY);
    if(fd == -1){
        perror("smash error: open failed");
        return;
    }
    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if(bytes_read == -1){
        perror("smash error: read failed");
        close(fd);
        return;
    }
    buffer[bytes_read] = '\0'; // Null-terminate the buffer
    if(close(fd) == -1){
        perror("smash error: close failed");
        return;
    }
    i = buffer;
    while(*i != '\0'){
        if(*i == '\n'){
            *i = '\0';
            break;
        }
        ++i;
    }
    string host_str = buffer;
    memset(buffer, 0, sizeof(buffer)); // clear buffer for next read

    fd = open("/proc/sys/kernel/osrelease", O_RDONLY);
    if(fd == -1){
        perror("smash error: open failed");
        return;
    }
    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if(bytes_read == -1){
        perror("smash error: read failed");
        close(fd);
        return;
    }
    buffer[bytes_read] = '\0'; // Null-terminate the buffer
    if(close(fd) == -1){
        perror("smash error: close failed");
        return;
    }
    i = buffer;
    while(*i != '\0'){
        if(*i == '\n'){
            *i = '\0';
            break;
        }
        ++i;
    }
    string kernel_str = buffer;
    memset(buffer, 0, sizeof(buffer)); // clear buffer for next read

    unsigned char header[64];

    fd = open("/proc/self/exe", O_RDONLY);
    if (fd == -1) {
        perror("smash error: open failed");
        return;
    }
    ssize_t n = read(fd, header, sizeof(header));
    if (n < 20) {  // צריך לפחות עד offset 20
        perror("smash error: read failed");
        close(fd);
        return;
    }
    if(close(fd) == -1){
        perror("smash error: close failed");
        return;
    }

    // making sure it's an ELF file
    if (header[0] != 0x7F || header[1] != 'E'  || header[2] != 'L'  || header[3] != 'F') return;

    uint16_t e_machine = (uint16_t)(header[18] | (header[19] << 8));
    string arch_str;
    switch (e_machine) {
        case 2:       // EM_SPARC
            arch_str = "sparc";
            break;
        case 3:       // EM_386
            arch_str = "x86";
            break;
        case 8:       // EM_MIPS
            arch_str = "mips";
            break;
        case 20:      // EM_PPC
            arch_str = "ppc";
            break;
        case 21:      // EM_PPC64
            arch_str = "ppc64";
            break;
        case 40:      // EM_ARM
            arch_str = "arm";
            break;
        case 62:      // EM_X86_64 / EM_AMD64
            arch_str = "x86_64";
            break;
        case 183:     // EM_AARCH64
            arch_str = "aarch64";
            break;
        case 243:     // EM_RISCV
            arch_str = "riscv";
            break;
        default:
            arch_str = "unknown";
            break;
    }

    char bigger_buffer[8192];

    fd = open("/proc/stat", O_RDONLY);
    if(fd == -1){
        perror("smash error: open failed");
        return;
    }
    bytes_read = read(fd, bigger_buffer, sizeof(bigger_buffer) - 1);
    if(bytes_read == -1){
        perror("smash error: read failed");
        close(fd);
        return;
    }
    bigger_buffer[bytes_read] = '\0'; // Null-terminate the buffer
    if(close(fd) == -1){
        perror("smash error: close failed");
        return;
    }

    char* line = bigger_buffer;
    time_t boot_time = -1;

    while (line && *line) {
        char* end = strchr(line, '\n');
        if (end) *end = '\0';

        if (strncmp(line, "btime ", 6) == 0) {
            // אחרי "btime " יש מספר
            char* p = line + 6;
            // לדלג על רווחים אם יש:
            while (*p == ' ' || *p == '\t') p++;
            long long val = strtoll(p, nullptr, 10);
            boot_time = (time_t)val;
            break;
        }

        if (!end) break;
        line = end + 1;
    }
    if(boot_time == -1){
        return;
    }
    struct tm tm_info;
    localtime_r(&boot_time, &tm_info);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_info);
    cout << "System: " << sys_str << endl;
    cout << "Hostname: " << host_str << endl;
    cout << "Kernel: " << kernel_str << endl;
    cout << "Architecture: " << arch_str << endl;
    cout << "Boot Time: " << time_buf << endl;

}






//=========================== class JobsList (and JobEntry) =============================//

JobsList::JobEntry::JobEntry(int jobId, Command* cmd) : jobId(jobId), cmd(cmd) {}

void JobsList::addJob(Command* cmd, bool isStopped){
    removeFinishedJobs();
    int jobId = getNextId();
    JobEntry job = JobEntry(jobId, cmd);
    job_entries.push_back(job);    
} 

void JobsList::printJobsList(){
    removeFinishedJobs();
    sortJobsById();
    for(const auto& job : job_entries){
        cout << "["<< job.jobId << "] " << job.cmd->getAliasCmdLine() << endl;
    }
}

void JobsList::killAllJobs(){
    int num_jobs = job_entries.size();
    std::cout << "smash: sending SIGKILL signal to " << num_jobs << " jobs:" << endl;
    if (num_jobs == 0) return;
    for(const auto& job : job_entries){
        pid_t pid = job.cmd->getPid();
        const char* cmdLine = job.cmd->getAliasCmdLine();
        cout << pid << ": " << cmdLine << endl;
        if (kill(pid, SIGKILL) == -1) {
            perror("smash error: kill failed");
        }
    }
    job_entries.clear();
}

void JobsList::removeFinishedJobs(){
    if(job_entries.empty()){
        return;
    }
    for(auto it = job_entries.begin(); it != job_entries.end(); ){
        if(it->cmd->isFinished() && !it->isStopped){
            it = job_entries.erase(it);  // erase returns the next valid iterator
        }else{
            ++it;
        }
    }
}

JobsList::JobEntry* JobsList::getJobById(int jobId){
    for(auto it = job_entries.begin(); it != job_entries.end(); ++it){
        if(it->jobId == jobId){
            return &(*it);
        }
    }
    return nullptr;
}

void JobsList::removeJobById(int jobId){
    for(auto it = job_entries.begin(); it != job_entries.end(); ++it){
        if(it->jobId == jobId){
            job_entries.erase(it);
            return;
        }
    }
}

void JobsList::sortJobsById(){
    sort(job_entries.begin(), job_entries.end(),
        [](const JobEntry& a, const JobEntry& b){
            return a.jobId < b.jobId;
        });
}

int JobsList::getNextId(){
    int nextId = 0;
    for(const auto& job : job_entries){
        if(job.jobId > nextId){
            nextId = job.jobId;
        }
    }
    return nextId + 1;
}


//=========================== class PipeCommand =============================//

PipeCommand::PipeCommand(const char* cmd_line) : Command(cmd_line){
    string cmd_s = string(cmd_line);
    if(cmd_s.find("|&") != string::npos){
        this->pipeType = 2;
        cmd_s.erase(cmd_s.find("&"), 1); // Remove the "&" sign if there is
    }
    string lcmd = cmd_s.substr(0, cmd_s.find_first_of("|"));
    string rcmd = cmd_s.substr(cmd_s.find_first_of("|") + 1);
    this->lcmd = SmallShell::getInstance().CreateCommand(lcmd.c_str());
    this->rcmd = SmallShell::getInstance().CreateCommand(rcmd.c_str());
}

PipeCommand::~PipeCommand(){
    delete lcmd;
    delete rcmd;
}

void PipeCommand::execute(){
    int pipefd[2];
    if(pipe(pipefd) == -1){
        perror("smash error: pipe failed");
        return;
    }
    pid_t lpid = fork();
    if(lpid == 0){
        dup2(pipefd[1], pipeType); //add  check if dup2 failed
        close(pipefd[0]);
        close(pipefd[1]);
        this->lcmd->execute();
        //close(pipeType); // close the write end of the pipe
        exit(0); // exit child process after executing the command
    }

    pid_t rpid = fork();
    if(rpid == 0){
        dup2(pipefd[0],0);
        close(pipefd[0]);
        close(pipefd[1]);
        this->rcmd->execute();
        close(0); // close the read end of the pipe
        exit(0); // exit child process after executing the command
    }
    //fflush(stdout);
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(lpid, nullptr, 0); // wait for the left command to finish
    waitpid(rpid, nullptr, 0); // wait for the right command to finish

}


//=========================== class SmallShell =============================//

SmallShell::SmallShell() {
    jobList = new JobsList();
}

SmallShell::~SmallShell() {
    if(lastDir != NULL){
        free(lastDir);
    }
    if(jobList != nullptr){
        delete jobList;
    }
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command* SmallShell::CreateCommand(const char* cmd_line) { // CREATE IT WITH ITS PID 
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    if(firstWord.compare("") == 0){
        return nullptr;
    }
    else if(firstWord.compare("pwd") == 0){
        return new GetCurrDirCommand(cmd_line);
    }
    else if(firstWord.compare("alias") == 0){
        return new AliasCommand(cmd_line, &alias, &reservedKeywords, &aliasKeys);
    }
    else if(cmd_s.find("|") != string::npos){
        return new PipeCommand(cmd_line);
    }
    else if(firstWord.compare("cd") == 0){
        return new ChangeDirCommand(cmd_line, &lastDir);
    }
    else if(firstWord.compare("quit") == 0){
        return new QuitCommand(cmd_line, jobList);
    }
    else if(firstWord.compare("kill") == 0){
        return new KillCommand(cmd_line, jobList);
    }
    else if(firstWord.compare("sysinfo") == 0){
        return new SysInfoCommand(cmd_line);
    }
    else if(firstWord.compare("whoami") == 0){
        return new WhoAmICommand(cmd_line);
    }
    else if(firstWord.compare("du") == 0){
        return new DiskUsageCommand(cmd_line);
    }
    else if(firstWord.compare("jobs") == 0){
        return new JobsCommand(cmd_line, jobList);
    }
    else if(firstWord.compare("fg") == 0){
        return new ForegroundCommand(cmd_line, jobList);
    }
    else if(firstWord.compare("unalias") == 0){
        return new UnAliasCommand(cmd_line, &alias, &aliasKeys);
    }
    else if(firstWord.compare("unsetenv") == 0){
        return new UnSetEnvCommand(cmd_line);
    }

    else if (firstWord.compare("chprompt") == 0) {
        char* args[COMMAND_MAX_ARGS];
        int numArgs = _parseCommandLine(cmd_line, args);     
        if(numArgs == 1) promptSetter("smash");
        else promptSetter(args[1]);
        return nullptr;
    }

  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }

  else if (this->alias.count(firstWord) > 0){
    cmd_s.replace(0, firstWord.length(), this->alias[firstWord]);
    char* c_str = new char[cmd_s.size() + 1];
    strcpy(c_str, cmd_s.c_str());
    Command* a_cmd = CreateCommand(c_str);
    if(a_cmd == nullptr){ // for some of the BuiltIn commands
        delete[] c_str;
        return nullptr;
    }
    a_cmd->setAliasCmdLine(cmd_line);
    return a_cmd;
  }

  else {
    return new ExternalCommand(cmd_line);
  }
 
  return nullptr;
}

void SmallShell::executeCommand(const char* cmd_line) {
    jobList->removeFinishedJobs();
    Command* cmd = CreateCommand(cmd_line);
    if(cmd == nullptr) return;

    bool is_background = _isBackgroundComamnd(cmd->getCmdLine()); // here new change
    char temp_str[strlen(cmd_line) + 1];
    strcpy(temp_str, cmd_line);

    std::string cmd_name = string(cmd_line);

    char* outp_red = strstr(temp_str, ">"); // Check if output redirection is needed
    int outp_red_type = 0; // 0 - no redirection, 1 - overwrite, 2 - append
    if (outp_red != NULL && cmd_name.find("unalias") == string::npos){
        if (strstr(outp_red, ">>") != NULL) {
            outp_red_type = 2; // append
        } else {
            outp_red_type = 1; // overwrite
        }
    }
    if(outp_red_type != 0 && is_background) {
        cmd_name = cmd_name.erase(cmd_name.find("&"), 1); // Remove the background sign if there is output redirection
        is_background = false; // If output redirection is needed, run in foreground
    }
    cmd_name.erase(0, cmd_name.rfind(">") + 1); // Remove everything before the first '<' (including it) to get the output file name
    cmd_name = _trim(cmd_name); // Trim the command name to remove leading and trailing spaces

    if(dynamic_cast<BuiltInCommand*>(cmd)){
        if(outp_red_type != 0){
            int fd = -1;
            if(outp_red_type == 1) fd = open(cmd_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
            else fd = open(cmd_name.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
            if(fd == -1){
                perror("smash error: open failed");
                return;
            }
            int stdout_copy = dup(STDOUT_FILENO); // Save the original stdout
            if(stdout_copy == -1){
                perror("smash error: dup failed");
                close(fd); // Close the file descriptor
                return;
            }
            if(dup2(fd, STDOUT_FILENO) == -1){ // Redirect stdout to the file
                perror("smash error: dup2 failed");
                close(fd); // Close the file descriptor
                return;
            }
            cmd->execute();
            fflush(stdout); // Flush the output buffer to ensure all data is written to the file
            close(fd); // Close the file descriptor
            
            if(dup2(stdout_copy, STDOUT_FILENO) == -1){ // Restore the original stdout
                perror("smash error: dup2 failed");
                return;
            };
            return;
        }
        else cmd->execute();
        return;
    }
    if(is_background == true){
        pid_t pid = fork();
        if(pid == 0){      // Child process
            setpgrp();     // Read more about this
            cmd->execute();
            exit(0);
        }
        else if(pid > 0){   // Parent process
            cmd->setPid(pid);
            jobList->addJob(cmd, false);
            return;
        }
        else{
            perror("smash error: fork failed");
        }
    }
    else if(is_background == false){  
        pid_t pid = fork();
        if(pid == 0){      // Child process
            setpgrp();
            if(outp_red_type != 0){
                int fd = -1;
                if(outp_red_type == 1) fd = open(cmd_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                else fd = open(cmd_name.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
                if(fd == -1){
                    perror("smash error: open failed");
                    exit(0);
                }
                if(dup2(fd, STDOUT_FILENO) == -1){ // Redirect stdout to the file
                    perror("smash error: dup2 failed");
                    close(fd); // Close the file descriptor
                    exit(EXIT_FAILURE); // is it neccsary ?
                }
                cmd->execute();
                close(fd); // Close the file descriptor
            }
            else cmd->execute();
            exit(0);
        }
        else if(pid > 0){   // Parent process
            cmd->setPid(pid);
            this->setForegPid(pid);
            int status;
            if(waitpid(pid, &status, WUNTRACED) == -1){
                perror("smash error: waitpid failed");
                return;
            }
            this->setForegPid(-1);
        }
        else{
            perror("smash error: fork failed");
        }
    }
}

