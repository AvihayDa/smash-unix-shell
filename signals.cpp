#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"


using namespace std;

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    pid_t foregPid = SmallShell::getInstance().getForegPid();
    if(foregPid != -1){
        if(kill(foregPid, SIGKILL) == -1){
            perror("smash error: kill failed");
            return;
        }
        cout << "smash: process " << foregPid << " was killed" << endl;
    }
}
