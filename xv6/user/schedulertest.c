#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int main() {
    int n, pid;
    int num_children = 5;

    for (n = 0; n < num_children; n++) {
        pid = fork();
        if (pid < 0) {
            break;
        }
        if (pid == 0) {
            if (n % 2 == 0) {
                // CPU-bound process
                volatile int counter = 0;
                for (int i = 0; i < 500000000; i++) {
                    counter++;
                }
            } else {
                // I/O-bound process (simulated by frequent yielding)
                for (int i = 0; i < 50; i++) {
                    pause(1);
                    volatile int counter = 0;
                    for (int j = 0; j < 1000000; j++) {
                        counter++;
                    }
                }
            }
            exit(0);
        }
    }

    for (n = 0; n < num_children; n++) {
        wait(0);
    }

    exit(0);
}
