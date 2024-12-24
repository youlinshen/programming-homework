#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define CD 5863276
#define LS 5863588
#define PRINTENV 7572809511666715
#define SETENV 6954013771834
#define HELP 6385292014
#define QUIT 6385632776

typedef struct commandType {
    char command[100];
    char parameter[100];
} command_t;

command_t *parser(char *commandStr) {
    command_t *cmd = (command_t *)malloc(sizeof(command_t));
    sscanf(commandStr, "%s", cmd->command);
    int x = strlen(cmd->command) + 1;
    sscanf(commandStr + x, "%[^\n]", cmd->parameter);
    return cmd;
}

const unsigned long hash(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

void changeDir(char *dir) {
    chdir(dir);
}

void printenv(char *arg) {
    printf("printenv %s\n", arg);
}

void mysetenv(char *arg) {
    printf("setenv %s\n", arg);
}

void help() {
    puts("\n WELCOME TO SHELL HELP"
         "\n commands supported"
         "\n>cd"
         "\n>setenv"
         "\n>printenv"
         "\n>quit"
         "\n>help");
}

void quit() {
    printf("\nGoodbye\n");
    exit(EXIT_SUCCESS);
}

// 处理外部命令
void execExternalCommand(command_t *cmd) {
    char *args[64] = {0};  // 用于存放命令及其参数
    char commandPath[256] = {0};
    int i = 0;

    // 获取外部命令的路径，例如从 bin 目录中查找
    snprintf(commandPath, sizeof(commandPath), "/home/shen/Documents/code/test/bin/%s", cmd->command);

    // 将命令本身作为 args[0]
    args[i++] = cmd->command;

    // 使用 strtok 分割参数
    char *token = strtok(cmd->parameter, " ");
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;  // 最后一个元素设置为 NULL，execvp 需要

    pid_t pid = fork();
    if (pid == 0) {
        // 子进程执行外部命令
        if (execvp(commandPath, args) == -1) {
            perror("execvp failed");
        }
        exit(1);  // 执行失败时退出
    } else if (pid < 0) {
        // fork 失败
        perror("fork failed");
    } else {
        // 父进程等待子进程结束
        wait(NULL);
    }
}

// 管道命令处理
void execPipeCommand(char *commandStr) {
    char *cmds[2]; // 用于存放两个命令
    pid_t pid1, pid2;
    int pipefd[2];  // 用于管道的文件描述符

    // 使用管道符（|）分割命令
    cmds[0] = strtok(commandStr, "|");
    cmds[1] = strtok(NULL, "|");

    if (cmds[0] != NULL && cmds[1] != NULL) {
        // 创建管道
        if (pipe(pipefd) == -1) {
            perror("pipe failed");
            return;
        }

        pid1 = fork();
        if (pid1 == 0) {
            // 第一个命令的子进程
            close(pipefd[0]); // 关闭管道的读端
            dup2(pipefd[1], STDOUT_FILENO); // 重定向标准输出到管道的写端
            close(pipefd[1]);
            
            command_t *cmd1 = parser(cmds[0]);
            execExternalCommand(cmd1);
            free(cmd1);
            exit(1);
        } else if (pid1 < 0) {
            perror("fork failed for command 1");
            return;
        }

        pid2 = fork();
        if (pid2 == 0) {
            // 第二个命令的子进程
            close(pipefd[1]); // 关闭管道的写端
            dup2(pipefd[0], STDIN_FILENO); // 重定向标准输入到管道的读端
            close(pipefd[0]);

            command_t *cmd2 = parser(cmds[1]);
            execExternalCommand(cmd2);
            free(cmd2);
            exit(1);
        } else if (pid2 < 0) {
            perror("fork failed for command 2");
            return;
        }

        // 父进程等待两个子进程
        close(pipefd[0]);
        close(pipefd[1]);
        waitpid(pid1, NULL, 0);
        waitpid(pid2, NULL, 0);
    } else {
        printf("Invalid pipe command\n");
    }
}

void Buildin(command_t *cmd) {
    switch (hash(cmd->command)) {
    case CD:
        changeDir(cmd->parameter);
        break;
    case PRINTENV:
        printenv(cmd->parameter);
        break;
    case SETENV:
        mysetenv(cmd->parameter);
        break;
    case HELP:
        help();
        break;
    case QUIT:
        quit();
        break;
    default:
        execExternalCommand(cmd);
        break;
    }
}

int main() {
    char commandStr[256] = {0};
    command_t *cmd = NULL;
    do {
        printf("MyShell%% ");
        fgets(commandStr, sizeof(commandStr), stdin);

        // 去掉换行符
        commandStr[strcspn(commandStr, "\n")] = 0;

        // 如果命令中包含管道符（|）
        if (strchr(commandStr, '|') != NULL) {
            execPipeCommand(commandStr);
        } else {
            cmd = parser(commandStr);
            Buildin(cmd);
            memset(cmd->parameter, 0, sizeof(cmd->parameter));
            free(cmd);
        }

    } while (strcmp(commandStr, "quit") != 0);

    return 0;
}
