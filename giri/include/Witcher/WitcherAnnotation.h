#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/wait.h>

void witcher_tx_begin() {
  // do nothing
}

void witcher_tx_end() {
  // do nothing
}

void witcher_get_memory_layout(char* dest) {
  char maps_path[30];
  sprintf(maps_path, "/proc/%d/maps", getpid());
  char * cp_args[5] = { "/bin/cp", "-r", maps_path, dest, NULL} ;

  pid_t c_pid, pid;
  int status;

  c_pid = fork();

  if (c_pid == 0){
    /* CHILD */
    execv(cp_args[0], cp_args);
    perror("execv failed");
  }else if (c_pid > 0){
    /* PARENT */
    if( (pid = wait(&status)) < 0){
      perror("wait");
      _exit(1);
    }
  }else{
    perror("fork failed");
    _exit(1);
  }
}
