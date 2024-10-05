#include <string.h> // For string operations
#include <unistd.h>
#include <limits.h> // Using this for PATH_MAX
#include <stdio.h>
#include <sys/times.h> // Using this for time
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#define LINE_LENGTH 100 // Max # of characters in an input line
#define MAX_ARGS    5   // Max number of arguments to a command
#define MAX_LENGTH  20  // Max # of characters in an argument
#define MAX_BG_PROC 1   // Max # of processes running in the background

pid_t running_process_pid = -1;
pid_t stopped_process_pid = -1;
int total_user_time = 0;
int total_system_time = 0;

extern char **environ;

// Handler for SIGCHLD (Child Process Terminated)
void handle_sigchld(int sig) {

    // Wait for any child process (non-blocking) with WNOHANG | WUNTRACED to handle stopped processes
    waitpid(-1, NULL, WNOHANG );
    running_process_pid = -1;  // Reset running process ID since the process is no longer active
}

// Handler for SIGINT (Ctrl+C)
void handle_sigint(int sig) {
  // Keep formatting consistent
  if (running_process_pid < 0) {
    printf("\ndragonshell > ");
  }
  else {
    printf("\n");
    running_process_pid = -1; // Reset running process ID
  }
  fflush(stdout);
  return;
}

// Handler for SIGTSTP (Ctrl+Z)
void handle_sigtstp(int sig) {
  // Keep formatting consistent
  if (running_process_pid < 0) {
    printf("\ndragonshell > ");
  }
  else {
    printf("\n");
    running_process_pid = -1; // Reset running process ID
  }
  fflush(stdout);
  return;
}

// Function to strip quotation marks from both ends of the string in place
void strip_quotes(char* str) {
  size_t len = strlen(str);

  // If the string has less than 2 characters, there's no need to strip anything
  if (len < 2) {
    return;
  }

  // Check if the string starts and ends with the same quote (either ' or ")
  if ((str[0] == '"' && str[len - 1] == '"') || (str[0] == '\'' && str[len - 1] == '\'')) {
    // Shift the entire string one character to the left to remove the first quote
    memmove(str, str + 1, len - 1);

    // Null-terminate the string by removing the last character (which is now the closing quote)
    str[len - 2] = '\0';
  }
}

/**
 * @brief Tokenize a C string 
 * 
 * @param str - The C string to tokenize 
 * @param delim - The C string containing delimiter character(s) 
 * @param argv - A char* array that will contain the tokenized strings
 * Make sure that you allocate enough space for the array.
 */
int tokenize(char* str, const char* delim, char ** argv){
  char* token;
  int i = 0;
  token = strtok(str, delim);
  while (token != NULL){
    strip_quotes(token);
    argv[i] = token;
    token = strtok(NULL, delim);
    i++;
  }
  argv[i] = NULL;
  return i;
}

int exit_shell(){
  return 0;

}

// Function to check if a pipe "|" exists in the tokens array
int is_pipe(char *tokens[]) {
  int i = 0;
  while (tokens[i] != NULL) {
    if (strcmp(tokens[i], "|") == 0) {
      return 1; // Pipe "|" found
    }
    i++;
  }
  return 0; // Pipe "|" not found
}

// Function to extract the program name from a given file path
char* extract_program_name(char *path) {
  // Use strrchr to find the last '/' in the path
  char *program_name = strrchr(path, '/');
  if (program_name != NULL) {
    // If a '/' was found, move one character ahead to get the program name
    return program_name + 1;
  } 
  else {
    // If no '/' was found, the path is the program name itself
    return path;
  }
}

// Function to count the number of elements in a char* array (e.g., argv[])
int get_length(char *argv[]) {
  int count = 0;
  // Loop through the array until we encounter NULL
  while (argv[count] != NULL) {
    count++;
  }
  return count;
}

// Function to print the current working directory (like the 'pwd' command)
// This is ran when pwd is entered into dragonshell
// If extra arguments are given, still returns current directory
int pwd(){
  char cwd[PATH_MAX]; // Buffer for current working directory
  if (getcwd(cwd, sizeof(cwd)) != NULL){
    printf("%s\n", cwd);
  } else {
    perror("getcwd() error");
  }
  return 0;
}

// Function to change the current working directory (like the 'cd' command)
// This function is ran when cd is entered into dragonshell
// This function expects an argument, otherwise returns errror to shell
// This function can recieve extra arguments
// This function expects a path given as argument path
int cd(char* path){
  // If path is \n, print "Expected argument to "cd"
  if (path == NULL || strlen(path) == 0){
    printf("dragonshell: Expected argument to \"cd\"\n");
  }
  // If else, try just opening given directory. This should work whether given relative path,
  // absolute path, or given .. to go to parent directory
  else if (chdir(path) == 0) {
    return 0; 
  } 
  // Path does not exist in current directory
  else {
    printf("dragonshell: No such file or directory\n");
  }
  return 0;
}

// Function to run a process with support for background execution, input/output redirection, and piping
// This function expects a command not built into the dragonshell already
// Path should be a path to an executable command that is accessible, argv should be a list of argument
// List of arguments can contain ">" and "<" for redirecting input and output,
// Can also contain "|" for piping processes
// argv will end with & if the process is to be run in the background
int run_process(char* path, char *argv[]){
  // We need to fork a new process and then run the new process using execve
  // If there are no arguments, make sure \0 is passed as the first element of argv

  
  int background_exec = 0;       // Flag to communicate if we need to execute in the background, 0 = No background execution, 1 = background execution
  char *redirect_output = NULL;  // To be string of file to redirect output to
  char *redirect_input = NULL;   // To be string of file to redirect input from
  int input_fdesc;               // File descriptor for input file
  int output_fdesc;              // File descriptor for output file
  char *pipe_to = NULL;          // To be string of file to redirect output using pipe
  int pipefd[2];                 // File descriptors for ends of pipe

  if (access(path, X_OK) != 0) {
    // Print error message and return if the file does not exist or is not executable
    printf("Command not found\n");
    return -1;
  }

  // Check if the last token in argv is "&"
  int arg_length = get_length(argv); // Get the number of arguments in argv[]
  
  if (arg_length > 0 && strcmp(argv[arg_length - 1], "&") == 0) {
    background_exec = 1; // Set the background execution flag
    argv[arg_length - 1] = NULL; // Remove the "&" from the arguments list
  }

  // We still need to do this for piping to check if parent has arguments

  char *arguments[arg_length];  // For the left side of the pipe
  char *arguments2[arg_length]; // Same size for the right side of the pipe (just to be safe)
  memset(arguments, 0, sizeof(arguments));  // Initialize both arrays to NULL
  memset(arguments2, 0, sizeof(arguments2));

  arguments[0] = extract_program_name(path); // First argument is the program name

  int i = 0;
  int j = 1; // Index for filling `arguments`
  int k = 1; // Index for filling `arguments2`
  int mode = 0; // If mode = 0, we're handling left side, 1 = right side

  while (argv[i] != NULL) {
    // If input parameters to program are coming from another file
    if (strcmp(argv[i], "<") == 0) {
      redirect_input = argv[i + 1];
      i += 2;
    }
    // If the output from the program is going into another file
    else if (strcmp(argv[i], ">") == 0) {
      redirect_output = argv[i + 1];
      i += 2;
    }
    // If we are piping from one process to another
    else if (strcmp(argv[i], "|") == 0) {
      mode = 1;
      pipe_to = argv[i + 1];
      arguments2[0] = extract_program_name(argv[i + 1]);  // Extract program name for pipe
      i += 2;
    }
    // Adds argument to argument list, based on mode
    else {
      if (mode == 0) {
        arguments[j++] = argv[i];  // Add to left-side arguments
      } 
      else {
        arguments2[k++] = argv[i]; // Add to right-side (pipe) arguments
      }
      i++;
    }
  }

  // Ensure both arrays are NULL-terminated
  arguments[j] = NULL;
  arguments2[k] = NULL;

  // If we do need to pipe, create pipe
  if (pipe_to != NULL) {
    pipe(pipefd);         // pipefd now contains file desciptors for pipe
  }

  pid_t pid = fork();
  running_process_pid = pid;

  if (pid == 0) {

        // Child process: reset signal handlers to default before executing the new program
    signal(SIGINT, SIG_DFL);    // Reset SIGINT to default action
    signal(SIGTSTP, SIG_DFL);   // Reset SIGTSTP to default action

    // There is a file that we need to read from
    if (redirect_input != NULL) {
      input_fdesc = open(redirect_input, O_RDONLY); // Grabs file descriptor for file that contains input by setting to read only
      dup2(input_fdesc, STDIN_FILENO);
      close(input_fdesc);
    }

    // There is a file that we need to write to
    if (redirect_output != NULL) {
      // Grabs file descriptor for file that we want to write to. If the file does not exist, it is created first. The code 0644 gives permission to create new file.
      output_fdesc = open(redirect_output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      dup2(output_fdesc, STDOUT_FILENO);
      close(output_fdesc);
    }

    // Redirect output to the pipe if there is a pipe
    if (pipe_to != NULL) {
      close(pipefd[0]);  // Close unused read end of the pipe
      dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to the pipe's write end
      close(pipefd[1]);  // Close write end after duplicating
    }

    // Run the program that was given with the arguments that were passed
    execve(path, arguments, environ);
    running_process_pid = -1;
    return 0;
  }

  else {
    // Parent process

    // We have to run right side process if we encountered pipe
    if (pipe_to != NULL) {
      // Fork again for the right-side of the pipe
      pid_t pid2 = fork();
      running_process_pid = pid2;

      // Child process for right-side of the pipe
      if (pid2 == 0) {

        // Child process: reset signal handlers to default before executing the new program
        signal(SIGINT, SIG_DFL);    // Reset SIGINT to default action
        signal(SIGTSTP, SIG_DFL);   // Reset SIGTSTP to default action
        
        // Redirect input from the pipe
        close(pipefd[1]);  // Close unused write end
        dup2(pipefd[0], STDIN_FILENO);  // Redirect stdin to pipe read end
        close(pipefd[0]);  // Close pipe read end after dup

        // Execute the right-side command
        execve(pipe_to, arguments2, environ);

      } 
      else {
        // Parent process closes pipe ends
        close(pipefd[0]);
        close(pipefd[1]);

        // Wait for both child processes
        waitpid(pid, NULL, WUNTRACED);
        waitpid(pid2, NULL, WUNTRACED);
      }
    } 
    else {
      // No pipe, just wait for the single child process
      if (background_exec == 0) {

        int status;
        waitpid(pid, &status, WUNTRACED);
        // If process was stopped by control Z
        if (WIFSTOPPED(status)) {
          stopped_process_pid = pid;
        }
        
      } 
      else {
        printf("PID %d is sent to background\n", pid);
      }
    }
  }
return 0;
}

// Function to gracefully exit the shell
int exit_dragonshell() {

  // Check if any running process needs to be terminated
  if (running_process_pid > 0) {
    kill(running_process_pid, SIGTERM); // Terminate process
  }
  // Check if any stopped process needs to be temrinated
  if (stopped_process_pid > 0) {
    kill(stopped_process_pid, SIGTERM); // Terminate process
  }

  // Struct to process time into
  struct tms total_time;
  times(&total_time);

  // Get number of ticks/sec
  long ticks = sysconf(_SC_CLK_TCK);

  // Compute and print total user and system time
  printf("User time: %.2f seconds\n", (double)total_time.tms_cutime / ticks);
  printf("System time: %.2f seconds\n", (double)total_time.tms_cstime / ticks);

  return 0;
}

// Runs the main dragonshell
// User can enter commands and arguments to be ran in shell:
// pwd - Print working directory
// cd path - Move working directory to path if it is accessible
// conmmand arg1 arg2... - Run command if accessible with arguments given
// command arg1 > output_file - Run command and output into output_file
// command arg1 < input_file - Rune command using input_file as args
// command arg & - Run command in background process
// command1 | command2 - Pipe output from command1 into input of command2
// exit - exit the program and print user time and system time used by children
// this shell will not be affected by SIGTSTP and SIGINT signals

int main(int argc, char *argv[]){

  printf("\nWelcome to Dragon Shell!\n\n");

  struct sigaction sa_chld, sa_int, sa_tstp;

  // Set up SIGCHLD handler
  sa_chld.sa_handler = &handle_sigchld;
  sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  sigemptyset(&sa_chld.sa_mask);
  sigaction(SIGCHLD, &sa_chld, NULL);

  // Set up SIGINT (Ctrl+C) handler
  sa_int.sa_handler = &handle_sigint;
  sa_int.sa_flags = SA_RESTART; // Restart interrupted system calls
  sigemptyset(&sa_int.sa_mask);
  sigaction(SIGINT, &sa_int, NULL);

  // Set up SIGTSTP (Ctrl+Z) handler
  sa_tstp.sa_handler = &handle_sigtstp;
  sa_tstp.sa_flags = SA_RESTART; // Restart interrupted system calls
  sigemptyset(&sa_tstp.sa_mask);
  sigaction(SIGTSTP, &sa_tstp, NULL);
    

  char str[LINE_LENGTH];    // String to hold untokenized string - need to edit size
  char* tokens[10]; // Array to hold tokens - need to edit size

  while (1){
      
  printf("dragonshell > ");
  fgets(str, sizeof(str), stdin); // Gets untokenized input from user

  // Replace \n with \0 to signify end of array
  size_t len = strlen(str);
  str[len - 1] = '\0';

  // Here we need to tokenize input, to seperate the arguments
  int token_length = tokenize(str, " ", tokens); 

  // Here we need to test for different cases

  // Case 1: exit is entered - shell termination, user stats are printed
  if (strcmp(tokens[0], "exit") == 0) {
    break;
  }
  
  // Case 2: pwd - print working directory
  else if (strcmp(tokens[0], "pwd") == 0){
    pwd();
  }

  // Case 3: cd path - change working directory
  else if (strcmp(tokens[0], "cd") == 0){
    if (token_length == 1)
    {
      cd("");      // If only cd is entered with no path
    } else {
      cd(tokens[1]); // If cd and path are entered
    }
  }

  // Case 4: A different comment is entered
  else {
    char **arguments = tokens + 1;
    run_process(tokens[0], arguments);
  }
  }
  exit_dragonshell();
}
