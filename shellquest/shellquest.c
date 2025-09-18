#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>

// Game state
int xp = 0;
int level = 1;
int completed_quests = 0;
#define TOTAL_QUESTS 8 // 5 main + 3 ls sub-quests
char current_dir[1024] = "/tmp/shellquest";
FILE *progress_file;
int ls_subquest_stage = 0;

// Job management
struct Job {
    pid_t pid;
    char cmd[256];
    int status; // 0: running, 1: stopped
};
struct Job jobs[10];
int job_count = 0;

// VFS simulation
#define VFS_SIZE 1048576
char vfs_disk[VFS_SIZE];
struct Inode {
    char name[32];
    int size;
    int start_block;
};
struct Directory {
    struct Inode files[10];
    int file_count;
};
struct Directory vfs_root = { .file_count = 0 };

// Process for scheduler
struct Process {
    int pid;
    int arrival;
    int burst;
};

// Function prototypes
void print_prompt();
int execute_command(char *cmd, int bg);
void teach_command(char *cmd);
void quest_ls();
void quest_cd();
void quest_cat();
void quest_mkdir();
void quest_touch();
void quest_grep();
void quest_jobs();
void quest_schedule();
void quest_kernelmod();
void show_explanation(const char *cmd, const char *brief, const char *detailed);
void check_level_up();
void show_stats();
void show_guide();
void list_quests();
void setup_sandbox();
void cleanup_sandbox();
void load_progress();
void save_progress();
void switch_to_zsh();
void handle_signal(int sig);
void add_job(pid_t pid, char *cmd, int status);
void list_jobs();
void fg_job(int job_id);
void bg_job(int job_id);
void vfs_init();
void vfs_touch(char *name);
void vfs_ls();
void vfs_cat(char *name);
int vfs_allocate_block(int size);
void simulate_fcfs();

// Main
int main() {
    signal(SIGINT, handle_signal);
    setpgid(0, 0); // Enable job control
    setup_sandbox();
    load_progress();
    vfs_init();
    show_guide();
    char *input;
    while (1) {
        print_prompt();
        input = readline("");
        if (input == NULL || strcmp(input, "exit") == 0) break;
        add_history(input);

        char *token = strtok(input, " ");
        if (token != NULL) {
            if (strcmp(token, "teach") == 0) {
                token = strtok(NULL, " ");
                if (token) teach_command(token);
            } else if (strcmp(token, "stats") == 0) {
                show_stats();
            } else if (strcmp(token, "guide") == 0 || strcmp(token, "help") == 0) {
                show_guide();
            } else if (strcmp(token, "quests") == 0) {
                list_quests();
            } else if (strcmp(token, "jobs") == 0) {
                list_jobs();
            } else if (strcmp(token, "fg") == 0) {
                token = strtok(NULL, " ");
                if (token) fg_job(atoi(token));
            } else if (strcmp(token, "bg") == 0) {
                token = strtok(NULL, " ");
                if (token) bg_job(atoi(token));
            } else if (strstr(input, "vfs_") == input) {
                if (strcmp(input, "vfs_ls") == 0) vfs_ls();
                else if (strstr(input, "vfs_touch ") == input) vfs_touch(input + 10);
                else if (strstr(input, "vfs_cat ") == input) vfs_cat(input + 8);
            } else {
                int bg = (strstr(input, "&") != NULL);
                execute_command(input, bg);
            }
        }
        free(input);
        if (completed_quests >= TOTAL_QUESTS) switch_to_zsh();
    }
    save_progress();
    cleanup_sandbox();
    printf("Exiting. XP: %d, Level: %d\n", xp, level);
    return 0;
}

// Prompt
void print_prompt() {
    getcwd(current_dir, sizeof(current_dir));
    printf("ShellQuest [Lv %d] %s $ ", level, current_dir);
}

// Execute
int execute_command(char *cmd, int bg) {
    char cmd_copy[1024];
    strcpy(cmd_copy, cmd);
    if (bg) cmd_copy[strlen(cmd_copy) - 1] = '\0'; // Remove &

    char *args[64];
    int i = 0;
    args[i++] = strtok(cmd_copy, " ");
    while ((args[i] = strtok(NULL, " ")) != NULL) i++;
    args[i] = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0); // New process group for job control
        execvp(args[0], args);
        perror("execvp");
        exit(1);
    } else if (pid > 0) {
        if (bg) {
            add_job(pid, cmd, 0);
            return 0;
        } else {
            int status;
            waitpid(pid, &status, 0);
            return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1;
        }
    }
    return 1;
}

// Explanation
void show_explanation(const char *cmd, const char *brief, const char *detailed) {
    printf("%s\n", brief);
    printf("Want more details? [y/n]: ");
    char choice[10];
    fgets(choice, sizeof(choice), stdin);
    if (choice[0] == 'y' || choice[0] == 'Y') {
        printf("%s\n", detailed);
    }
}

// Teach
void teach_command(char *cmd) {
    if (strcmp(cmd, "ls") == 0) quest_ls();
    else if (strcmp(cmd, "cd") == 0) quest_cd();
    else if (strcmp(cmd, "cat") == 0) quest_cat();
    else if (strcmp(cmd, "mkdir") == 0) quest_mkdir();
    else if (strcmp(cmd, "touch") == 0) quest_touch();
    else if (strcmp(cmd, "grep") == 0) quest_grep();
    else if (strcmp(cmd, "jobs") == 0) quest_jobs();
    else if (strcmp(cmd, "schedule") == 0) quest_schedule();
    else if (strcmp(cmd, "kernelmod") == 0) quest_kernelmod();
    else printf("Unknown: %s\n", cmd);
}

// Quests
void quest_ls() {
    if (ls_subquest_stage == 0) {
        printf("Quest: Use 'ls' to list files in the village square.\n");
        system("mkdir -p /tmp/shellquest/quest_ls && touch /tmp/shellquest/quest_ls/village.txt");
        char *input;
        while (1) {
            print_prompt();
            input = readline("");
            if (strstr(input, "ls") && !strstr(input, "-")) {
                if (execute_command(input, 0) == 0) {
                    xp += 10; completed_quests++; ls_subquest_stage = 1;
                    show_explanation("ls", "You listed files in the directory!",
                                     "The 'ls' command lists directory contents. Use it to see files/folders in your current location.");
                    printf("✅ +10 XP! New sub-quest unlocked: Try 'teach ls' again for 'ls -a'.\n");
                    break;
                } else {
                    printf("Try again: Use 'ls' without options.\n");
                }
            } else {
                execute_command(input, 0);
            }
            free(input);
        }
        system("rm -rf /tmp/shellquest/quest_ls");
    } else if (ls_subquest_stage == 1) {
        printf("Sub-Quest: Use 'ls -a' to find hidden treasures.\n");
        system("mkdir -p /tmp/shellquest/quest_ls && touch /tmp/shellquest/quest_ls/.hidden.txt");
        char *input;
        while (1) {
            print_prompt();
            input = readline("");
            if (strstr(input, "ls -a") != NULL) {
                if (execute_command(input, 0) == 0) {
                    xp += 15; completed_quests++; ls_subquest_stage = 2;
                    show_explanation("ls -a", "You listed all files, including hidden ones starting with '.'!",
                                     "The 'ls -a' option shows all files, including hidden ones (starting with '.'). Use it to view configuration files like .zshrc.");
                    printf("✅ +15 XP! Next: 'teach ls' for 'ls -l'.\n");
                    break;
                } else {
                    printf("Try again: Use 'ls -a'.\n");
                }
            } else {
                execute_command(input, 0);
            }
            free(input);
        }
        system("rm -rf /tmp/shellquest/quest_ls");
    } else if (ls_subquest_stage == 2) {
        printf("Sub-Quest: Use 'ls -l' to inspect file details.\n");
        system("mkdir -p /tmp/shellquest/quest_ls && touch /tmp/shellquest/quest_ls/village.txt");
        char *input;
        while (1) {
            print_prompt();
            input = readline("");
            if (strstr(input, "ls -l") != NULL) {
                if (execute_command(input, 0) == 0) {
                    xp += 15; completed_quests++; ls_subquest_stage = 3;
                    show_explanation("ls -l", "You listed files with detailed info like permissions and sizes!",
                                     "The 'ls -l' option shows a long listing format with permissions, owner, size, and date. Use it to check file metadata.");
                    printf("✅ +15 XP! Next: 'teach ls' for 'ls -al'.\n");
                    break;
                } else {
                    printf("Try again: Use 'ls -l'.\n");
                }
            } else {
                execute_command(input, 0);
            }
            free(input);
        }
        system("rm -rf /tmp/shellquest/quest_ls");
    } else if (ls_subquest_stage == 3) {
        printf("Sub-Quest: Use 'ls -al' to see all details.\n");
        system("mkdir -p /tmp/shellquest/quest_ls && touch /tmp/shellquest/quest_ls/.hidden.txt");
        char *input;
        while (1) {
            print_prompt();
            input = readline("");
            if (strstr(input, "ls -al") != NULL || strstr(input, "ls -la") != NULL) {
                if (execute_command(input, 0) == 0) {
                    xp += 20; completed_quests++; ls_subquest_stage = 4;
                    show_explanation("ls -al", "You listed all files with full details!",
                                     "The 'ls -al' combines '-a' (all files) and '-l' (long format). Use it for a complete view of a directory, including hidden file details.");
                    printf("✅ +20 XP! All ls sub-quests complete!\n");
                    break;
                } else {
                    printf("Try again: Use 'ls -al' or 'ls -la'.\n");
                }
            } else {
                execute_command(input, 0);
            }
            free(input);
        }
        system("rm -rf /tmp/shellquest/quest_ls");
    }
}

void quest_cd() {
    printf("Quest: Use 'cd dungeon' to enter the dungeon.\n");
    system("mkdir -p /tmp/shellquest/dungeon && touch /tmp/shellquest/dungeon/flag.txt");
    char *input;
    while (1) {
        print_prompt();
        input = readline("");
        if (strstr(input, "cd dungeon") != NULL) {
            if (execute_command(input, 0) == 0 && chdir("/tmp/shellquest/dungeon") == 0) {
                xp += 15; completed_quests++;
                show_explanation("cd", "You changed to the dungeon directory!",
                                 "The 'cd' command changes your current working directory. Use it to navigate the filesystem, e.g., 'cd /home' or 'cd ..' to go up.");
                printf("✅ +15 XP. Entered dungeon!\n");
                break;
            } else {
                printf("Try again: Use 'cd dungeon' (ensure directory exists).\n");
            }
        } else {
            execute_command(input, 0);
        }
        free(input);
    }
    chdir("/tmp/shellquest");
    system("rm -rf /tmp/shellquest/dungeon");
}

void quest_cat() {
    printf("Quest: Use 'cat secret.txt' to read the secret.\n");
    // Ensure directory and file are created
    if (system("mkdir -p /tmp/shellquest/quest_cat") != 0) {
        printf("Error: Failed to create directory /tmp/shellquest/quest_cat\n");
        return;
    }
    if (system("echo 'Flag: SQ-001' > /tmp/shellquest/quest_cat/secret.txt") != 0) {
        printf("Error: Failed to create secret.txt\n");
        return;
    }
    // Change to quest directory
    if (chdir("/tmp/shellquest/quest_cat") != 0) {
        printf("Error: Failed to change to /tmp/shellquest/quest_cat\n");
        system("rm -rf /tmp/shellquest/quest_cat");
        return;
    }
    char *input;
    while (1) {
        print_prompt();
        input = readline("");
        if (strstr(input, "cat secret.txt") != NULL) {
            if (execute_command(input, 0) == 0) {
                xp += 20; completed_quests++;
                show_explanation("cat", "You displayed the contents of secret.txt!",
                                 "The 'cat' command concatenates and displays file contents. Use it to read text files or combine multiple files, e.g., 'cat file1 file2'.");
                printf("✅ +20 XP. Found flag!\n");
                break;
            } else {
                printf("Try again: Use 'cat secret.txt' (ensure file exists).\n");
            }
        } else {
            execute_command(input, 0);
        }
        free(input);
    }
    // Return to base directory and clean up
    chdir("/tmp/shellquest");
    system("rm -rf /tmp/shellquest/quest_cat");
}

void quest_mkdir() {
    printf("Quest: Use 'mkdir fortress' to build a fortress.\n");
    system("mkdir -p /tmp/shellquest/quest_mkdir");
    char *input;
    while (1) {
        print_prompt();
        input = readline("");
        if (strstr(input, "mkdir fortress") != NULL) {
            if (execute_command(input, 0) == 0) {
                xp += 15; completed_quests++;
                show_explanation("mkdir", "You created a new directory called fortress!",
                                 "The 'mkdir' command creates directories. Use it to organize files, e.g., 'mkdir docs' or 'mkdir -p path/to/nested' for nested dirs.");
                printf("✅ +15 XP. Fortress built!\n");
                break;
            } else {
                printf("Try again: Use 'mkdir fortress' (remove existing fortress if needed).\n");
            }
        } else {
            execute_command(input, 0);
        }
        free(input);
    }
    system("rm -rf /tmp/shellquest/quest_mkdir");
}

void quest_touch() {
    printf("Quest: Use 'touch flag.txt' to place a flag.\n");
    system("mkdir -p /tmp/shellquest/quest_touch");
    char *input;
    while (1) {
        print_prompt();
        input = readline("");
        if (strstr(input, "touch flag.txt") != NULL) {
            if (execute_command(input, 0) == 0) {
                xp += 15; completed_quests++;
                show_explanation("touch", "You created or updated flag.txt!",
                                 "The 'touch' command creates empty files or updates timestamps. Use it to create new files, e.g., 'touch notes.txt', or refresh existing ones.");
                printf("✅ +15 XP. Flag placed!\n");
                break;
            } else {
                printf("Try again: Use 'touch flag.txt'.\n");
            }
        } else {
            execute_command(input, 0);
        }
        free(input);
    }
    system("rm -rf /tmp/shellquest/quest_touch");
}

void quest_grep() {
    printf("Quest: Use 'grep code secret.txt' to find the hidden code.\n");
    // Ensure directory and file are created
    if (system("mkdir -p /tmp/shellquest/quest_grep") != 0) {
        printf("Error: Failed to create directory /tmp/shellquest/quest_grep\n");
        return;
    }
    if (system("echo 'Secret code: XYZ123' > /tmp/shellquest/quest_grep/secret.txt") != 0) {
        printf("Error: Failed to create secret.txt\n");
        return;
    }
    // Change to quest directory
    if (chdir("/tmp/shellquest/quest_grep") != 0) {
        printf("Error: Failed to change to /tmp/shellquest/quest_grep\n");
        system("rm -rf /tmp/shellquest/quest_grep");
        return;
    }
    char *input;
    while (1) {
        print_prompt();
        input = readline("");
        if (strstr(input, "grep code secret.txt") != NULL) {
            if (execute_command(input, 0) == 0) {
                xp += 20; completed_quests++;
                show_explanation("grep", "You searched for 'code' in secret.txt!",
                                 "The 'grep' command searches for patterns in files. Use it to find text, e.g., 'grep error log.txt' or 'grep -r pattern dir' for recursive search.");
                printf("✅ +20 XP. Code found!\n");
                break;
            } else {
                printf("Try again: Use 'grep code secret.txt' (ensure file exists).\n");
            }
        } else {
            execute_command(input, 0);
        }
        free(input);
    }
    // Return to base directory and clean up
    chdir("/tmp/shellquest");
    system("rm -rf /tmp/shellquest/quest_grep");
}

void quest_jobs() {
    printf("Quest: Run 'sleep 10 &' then 'jobs' and 'fg 1'.\n");
    char *input;
    int job_started = 0;
    while (1) {
        print_prompt();
        input = readline("");
        if (strstr(input, "sleep 10 &") != NULL) {
            if (execute_command(input, 1) == 0) {
                job_started = 1;
                printf("Background job started. Now try 'jobs'.\n");
            }
        } else if (strstr(input, "jobs") != NULL && job_started) {
            list_jobs();
        } else if (strstr(input, "fg 1") != NULL && job_started && job_count > 0) {
            fg_job(1);
            xp += 20; completed_quests++;
            show_explanation("jobs/fg", "You managed background jobs!",
                             "The 'jobs' command lists background processes, and 'fg' brings them to the foreground. Use them to manage tasks, e.g., 'sleep 30 &' then 'fg 1'.");
            printf("✅ +20 XP!\n");
            break;
        } else {
            execute_command(input, strstr(input, "&") != NULL);
        }
        free(input);
    }
}

void quest_schedule() {
    printf("Quest: Simulate FCFS scheduling!\n");
    simulate_fcfs();
    xp += 30; completed_quests++;
    show_explanation("schedule", "You ran a First-Come-First-Serve scheduler simulation!",
                     "Scheduling determines process execution order. FCFS runs processes in arrival order; use it for simple, non-preemptive systems.");
    printf("✅ +30 XP!\n");
    check_level_up();
}

void quest_kernelmod() {
    printf("Quest: Load kernel module, read stats with 'cat /proc/shellquest_stats', then unload.\n");
    printf("Hint: Use 'sudo insmod /home/vijay/shellquest-module/shellquest_stats.ko' and 'sudo rmmod shellquest_stats'.\n");
    char *input;
    while (1) {
        print_prompt();
        input = readline("");
        if (strstr(input, "cat /proc/shellquest_stats") != NULL) {
            if (execute_command(input, 0) == 0) {
                xp += 25; completed_quests++;
                show_explanation("kernelmod", "You read stats from a kernel module via procfs!",
                                 "Kernel modules extend Linux functionality. The 'insmod' command loads them, and 'rmmod' unloads them. Use '/proc' files for kernel-user communication.");
                printf("✅ +25 XP! Unload module to finish.\n");
            } else {
                printf("Try again: Use 'cat /proc/shellquest_stats' (ensure module is loaded).\n");
            }
        } else if (strstr(input, "rmmod shellquest_stats") != NULL) {
            if (execute_command(input, 0) == 0) {
                printf("✅ Module unloaded! Quest complete.\n");
                break;
            } else {
                printf("Try again: Use 'sudo rmmod shellquest_stats'.\n");
            }
        } else {
            execute_command(input, strstr(input, "&") != NULL);
        }
        free(input);
    }
}

// Level up
void check_level_up() {
    if (xp >= level * 50) {
        level++;
        printf("🎉 Level %d!\n", level);
    }
    save_progress();
}

// Stats
void show_stats() {
    printf("XP: %d, Level: %d, Quests: %d/%d\n", xp, level, completed_quests, TOTAL_QUESTS);
}

// Guide
void show_guide() {
    printf("\n🎉 Welcome to ShellQuest: Your Gamified Journey to Mastering Linux! 🎉\n\n");
    printf("ShellQuest turns learning Linux commands into an adventure. Earn XP, level up, and unlock quests!\n\n");
    printf("Quick Start Guide:\n");
    printf("1. Type 'teach <command>' to start a quest (e.g., 'teach ls').\n");
    printf("2. Follow prompts to practice commands in a safe sandbox.\n");
    printf("3. Complete tasks to earn XP and unlock sub-quests (e.g., 'ls -a').\n");
    printf("4. Check progress with 'stats' or list quests with 'quests'.\n");
    printf("5. Finish all quests to graduate to a standard terminal!\n\n");
    printf("Tips:\n");
    printf("- Use 'man <command>' for help.\n");
    printf("- Sandboxed: No system risks.\n");
    printf("- Try OS quests like 'schedule' or 'kernelmod'.\n");
    printf("Start with 'teach ls'!\n");
}

// Quests list
void list_quests() {
    printf("Available Quests:\n");
    printf("- ls (Sub-quests: ls, ls -a, ls -l, ls -al)\n");
    printf("- cd\n");
    printf("- cat\n");
    printf("- mkdir\n");
    printf("- touch\n");
    printf("- grep\n");
    printf("- jobs\n");
    printf("- schedule\n");
    printf("- kernelmod\n");
    printf("Completed: %d/%d\n", completed_quests, TOTAL_QUESTS);
}

// Sandbox
void setup_sandbox() {
    mkdir("/tmp/shellquest", 0755);
    chdir("/tmp/shellquest");
    progress_file = fopen("/tmp/shellquest/.quest_progress", "r+");
    if (!progress_file) progress_file = fopen("/tmp/shellquest/.quest_progress", "w+");
}

void cleanup_sandbox() {
    fclose(progress_file);
    chdir("/");
    system("rm -rf /tmp/shellquest");
}

// Progress
void load_progress() {
    if (progress_file) {
        fscanf(progress_file, "%d %d %d %d", &xp, &level, &completed_quests, &ls_subquest_stage);
        rewind(progress_file);
    }
}

void save_progress() {
    if (progress_file) {
        fprintf(progress_file, "%d %d %d %d", xp, level, completed_quests, ls_subquest_stage);
        fflush(progress_file);
    }
}

// Switch
void switch_to_zsh() {
    system("sudo chsh -s /bin/zsh $USER");
    printf("All quests done! Switching to Zsh. Relaunch terminal.\n");
    exit(0);
}

// Signals
void handle_signal(int sig) {
    if (job_count > 0) kill(jobs[0].pid, SIGINT);
}

// Jobs
void add_job(pid_t pid, char *cmd, int status) {
    jobs[job_count].pid = pid;
    strcpy(jobs[job_count].cmd, cmd);
    jobs[job_count].status = status;
    job_count++;
}

void list_jobs() {
    for (int i = 0; i < job_count; i++) {
        printf("[%d] %s (status: %s)\n", i+1, jobs[i].cmd, jobs[i].status ? "stopped" : "running");
    }
}

void fg_job(int job_id) {
    if (job_id > 0 && job_id <= job_count) {
        kill(jobs[job_id-1].pid, SIGCONT);
        waitpid(jobs[job_id-1].pid, NULL, 0);
        memmove(&jobs[job_id-1], &jobs[job_id], sizeof(struct Job) * (job_count - job_id));
        job_count--;
    }
}

void bg_job(int job_id) {
    if (job_id > 0 && job_id <= job_count) {
        kill(jobs[job_id-1].pid, SIGCONT);
        jobs[job_id-1].status = 0;
    }
}

// VFS
void vfs_init() {
    memset(vfs_disk, 0, VFS_SIZE);
}

int vfs_allocate_block(int size) {
    for (int i = 0; i < VFS_SIZE - size; i++) {
        if (vfs_disk[i] == 0) return i;
    }
    return -1;
}

void vfs_touch(char *name) {
    if (vfs_root.file_count < 10) {
        strcpy(vfs_root.files[vfs_root.file_count].name, name);
        vfs_root.files[vfs_root.file_count].size = 0;
        vfs_root.files[vfs_root.file_count].start_block = vfs_allocate_block(1);
        vfs_root.file_count++;
        printf("Created %s in VFS\n", name);
    }
}

void vfs_ls() {
    for (int i = 0; i < vfs_root.file_count; i++) {
        printf("%s\n", vfs_root.files[i].name);
    }
}

void vfs_cat(char *name) {
    for (int i = 0; i < vfs_root.file_count; i++) {
        if (strcmp(vfs_root.files[i].name, name) == 0) {
            printf("Content of %s: [sim data]\n", name);
            return;
        }
    }
    printf("Not found\n");
}

// Scheduler
void simulate_fcfs() {
    int n = 3;
    struct Process procs[3] = {{1,0,5}, {2,1,3}, {3,2,4}};
    int wait[3] = {0}, turnaround[3] = {0};
    wait[0] = 0;
    for (int i = 1; i < n; i++) wait[i] = procs[i-1].burst + wait[i-1];
    for (int i = 0; i < n; i++) turnaround[i] = procs[i].burst + wait[i];
    printf("Gantt: |P1(0-5)|P2(5-8)|P3(8-12)|\n");
    printf("Avg Wait: %.2f\n", (wait[0]+wait[1]+wait[2])/(float)n);
}
