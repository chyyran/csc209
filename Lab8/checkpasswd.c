#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAXLINE 256
#define MAX_PASSWORD 10

#define SUCCESS "Password verified\n"
#define INVALID "Invalid password\n"
#define NO_USER "No such user\n"

int main(void)
{
    char user_id[MAXLINE];
    char password[MAXLINE];
    char buf[MAX_PASSWORD];
    int pipefd[2];
    pipe(pipefd);

    if (fgets(user_id, MAXLINE, stdin) == NULL)
    {
        perror("fgets");
        exit(1);
    }
    if (fgets(password, MAXLINE, stdin) == NULL)
    {
        perror("fgets");
        exit(1);
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        exit(1);
    }

    if (pid != 0)
    {
        close(pipefd[0]);
        memset(buf, 0, MAX_PASSWORD);
        memcpy(buf, user_id, MAX_PASSWORD);

        write(pipefd[1], buf, MAX_PASSWORD);

        memset(buf, 0, MAX_PASSWORD);
        memcpy(buf, password, MAX_PASSWORD);

        write(pipefd[1], buf, MAX_PASSWORD);
    }

    if (pid == 0)
    {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        int pipefd2[2];
        pipe(pipefd2);

        pid_t _pid = fork();

        if (_pid < 0)
        {
            perror("fork");
            exit(1);
        }

        if (_pid != 0)
        {
            int result;
            waitpid(pid, &result, 0);
            switch (WEXITSTATUS(result))
            {
            case 0:
                printf(SUCCESS);
                break;
            case 1:
                break;
            case 2:
                printf(INVALID);
                break;
            case 3:
                printf(NO_USER);
                break;
            }
        }

        if (_pid == 0) {
            execl("./validate", "validate", NULL);
        }
    }

    return 0;
}
