#include "judger.h"

char * const TEMP_DIR_TEMPLATE = "/tmp/judgetmp.XXXXXX";
char TEMP_DIR[] = "/tmp/judgetmp.XXXXXX";

/*
enum lang{
    C=0,
    CXX,
    PASCAL,
    JAVA,
    CSHARP,
    OBJC,
    PYTHON2,
    PYTHON3,
    RUBY,
    OCTAVE,
};
 */

char *code_file_name[]={
        "main.c",
        "main.cpp",
        "main.pp",
        "Main.java",
        "main.cs",
        "main.objc",
        "main.py",
        "main.py",
        "main.rb",

};

char *compile_cmd[][20]={
        {"gcc",
                "-static",
                "-fno-optimize-sibling-calls",
                "-fno-strict-aliasing",
                "-DONLINE_JUDGE",
                "-fno-asm",
                "-lm",
                "-s",
                //"-Wl,--stack=268435456",
                "-O2",
                "-o", "main","main.c", NULL},
        {"g++",
                "-static",
                "-DONLINE_JUDGE",
                "-lm",
                "-s",
                "-x",
                "c++",
                "-O2",
                "-std=c++11",
                "-o", "main","main.cpp", NULL},
        {"fpc",
                "main.pp", NULL},
        {"javac",
                "Main.java", NULL},
        {"mcs",
                "main.cs", NULL},
        {NULL},
        {NULL},
        {NULL},
};

char *run_cmd[][10]={
        {"./main", NULL},
        {"./main", NULL},
        {"./main", NULL},
        {"java", "Main", NULL},
        {"mono", "main.exe", NULL},
        {NULL},
        {"python","main.py" ,NULL},
        {"python3","main.py" ,NULL},
};

void get_file_fd(uint probelm_id, int * fd, const char * file_path, const char * target_name, int mode)
{
    char file_name[30];
    unsigned long len;
    strcpy(file_name, file_path);
    len = strlen(file_name);
    sprintf(file_name+len, "/%d/", probelm_id);
    len = strlen(file_name);
    sprintf(file_name+len, "%s", target_name);
    *fd = open(file_name, mode);
    syslog(LOG_INFO, "open file %s", file_name);
}

void prepare_files(run_param* run, int * input_fd, int * output_fd, int * code_fd)
{
    get_file_fd(run->problem_id, input_fd, INPUT_DIR, "test.in", O_RDONLY);
    *output_fd = open("out.out", O_WRONLY|O_CREAT, S_IRWXU| S_IRWXG| S_IRWXO);
    ftruncate(*output_fd, 0);
    syslog(LOG_DEBUG, "prob num : %d.",run->problem_id);
    syslog(LOG_DEBUG, "code file : %s.",code_file_name[run->lang]);
    *code_fd = open(code_file_name[run->lang], O_WRONLY|O_CREAT);
    ftruncate(*code_fd, 0);
};

int judge(run_param * run)
{
    //run_result result;
    char * temp_dir;
    //struct passwd *nobody= getpwnam("nobody");
    int pid, null_fd;
    int ret;
    int status;
    
    uid_t nobody_uid;
    gid_t nobody_gid;
    struct passwd *nobody;
    
    int input_fd, output_fd, code_fd;

    strcpy(TEMP_DIR, TEMP_DIR_TEMPLATE);
    temp_dir = mkdtemp(TEMP_DIR);
    if (temp_dir == NULL) {
        syslog(LOG_ERR, "err creating temp dir. %s", strerror(errno));
        return -1;
    }
    syslog(LOG_DEBUG, "temp_dir made %s.", TEMP_DIR);
    
    chdir(temp_dir);
    
    prepare_files(run, &input_fd, &output_fd, &code_fd);
    if (input_fd <0 || output_fd<0 || code_fd<0)
    {
        syslog(LOG_ERR, "file prepare failed. %s", strerror(errno));
    }
    syslog(LOG_DEBUG, "input_fd : %d", input_fd);
    syslog(LOG_DEBUG, "output_fd : %d", output_fd);

    ret = (int)write(code_fd, run->code, strlen(run->code));
    if (ret < 0) {
        syslog(LOG_ERR, "code write err. %s", strerror(errno));
        return -1;
    }
    close(code_fd);
    syslog(LOG_DEBUG, "compile_cmd %ld", compile_cmd[run->lang][0]);
    if (compile_cmd[run->lang][0]!= NULL) {
        //compile code
        pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR,"judge compile fork error");
            return -1;
        }
        if (pid == 0) {
            ret = execvp(compile_cmd[run->lang][0],compile_cmd[run->lang]);
            if (ret < 0){
                syslog(LOG_ERR, "exec err: %s.\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        else {
            waitpid(pid, &status, WUNTRACED | WCONTINUED);
            int cs = parse_status(status);
            syslog(LOG_DEBUG, "code compile status code:%d\n",cs);
        }

    }


    null_fd = open("/dev/null", O_RDWR);
    syslog(LOG_DEBUG, "null_fd : %d", null_fd);

    nobody = getpwnam("nobody");
    if (nobody == NULL) {
        syslog(LOG_ERR, "find user nobody failed.\n");
        exit(EXIT_FAILURE);
    }
    nobody_uid = nobody->pw_uid;
    nobody_gid = nobody->pw_gid;

    //char *argv[] = {"python", "main.py", NULL};
    //ret = execv("code", argv);
    //syslog(LOG_ERR, "run code error.\n");
    //ret = execvp(run_cmd[run->lang][0], run_cmd[run->lang]);
    //ret = execvp("python", argv);
    //ret = execvp(run_cmd[run->lang][0], run_cmd[run->lang]);

    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR,"judge fork error");
        return -1;
    }
    if (pid == 0) {
        //child
        /*ret = chroot(temp_dir);
        if (ret < 0) {
            syslog(LOG_ERR, "chroot failed. %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        chdir("/");*/
        
        dup2(null_fd, STDERR_FILENO);
        dup2(input_fd, STDIN_FILENO);
        dup2(output_fd, STDOUT_FILENO);
        syslog(LOG_DEBUG, "start ptrace");
        setuid(nobody_uid);
        setgid(nobody_gid);

        ret = (int) ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        if (ret < 0){
            syslog(LOG_ERR, "Error initiating ptrace");
        }
        //char *argv[] = {NULL};
        //ret = execvp("./main", argv);
        execvp(run_cmd[run->lang][0], run_cmd[run->lang]);
        syslog(LOG_ERR, "run code error.\n");
        //ret = execvp(run_cmd[run->lang][0], run_cmd[run->lang]);
    }
    else if (pid > 0){
        //parent


        struct rusage usage;
        syslog(LOG_DEBUG, "start watching");
        while(1) { //listening
            wait4(pid, &status, WUNTRACED | WCONTINUED, &usage);
            int cs = parse_status(status);
            //int time = tv2ms(usage.ru_utime) + tv2ms(usage.ru_stime);
            //long memory_now = usage.ru_minflt * (getpagesize() >> 10);
            //if (memory_now > memory_max)
            //    memory_max = memory_now;
            /*if ((time > timeLimit) || timeout_killed) {
                printf("Time Limit Exceeded\n");
                ptrace(PTRACE_KILL, child_pid, NULL, NULL);
                return TLE;
            }
            if (memory_max > memoryLimit) {
                printf("Memory Limit Exceeded\n");
                ptrace(PTRACE_KILL, child_pid, NULL, NULL);
                return MLE;
            }*/
            if (cs == PS_EXIT_ERROR) { //exited
                //printf("%d %dms %ldKiB\n", WEXITSTATUS(status), time, memory_max);
                syslog(LOG_DEBUG, "program exited error.\n");
                return FIN;
            }
            if (cs == PS_SYSCALL) {
                ret = syscall_checker(pid);
                if (ret == SS_FORBIDDEN) {
                    syslog(LOG_INFO, "forbid syscall");
                    ptrace(PTRACE_KILL, pid, NULL, NULL);
                    return RE;
                }
            }
            if (cs == PS_EXIT_SUCCESS) {
                syslog(LOG_DEBUG, "program exited successfully.\n");
                return FIN;
            }

            ret = (int) ptrace(PTRACE_SYSCALL, pid, NULL, NULL);
            if (ret < -1) {
                syslog(LOG_ERR, "Trace again failed.\n");
                return RE;
            }
        }
    }
    
    return 0;
}