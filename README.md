#----------------------------- 
# Name : Shawn Belhumeur
# SID : 1599609
# CCID : sdbelhum
#-----------------------------

# Dragonshell

A simple Unix-like shell that supports basic shell functionalities such as command execution, piping, redirection, background processes, and built-in commands like pwd and cd.

## Features

Command Execution: Run external commands and programs with arguments.
Built-in Commands:
pwd: Print the current working directory.
cd <path>: Change the current directory.
Background Execution: Run processes in the background by appending & to a command.
Input and Output Redirection:
Redirect output using > to a file.
Redirect input using < from a file.
Piping: Chain commands using | to send the output of one command as input to another.
Graceful Termination: Supports graceful exit with the exit command, displaying user and system times.
Signal Handling: Handles SIGINT (Ctrl+C) and SIGTSTP (Ctrl+Z) to interrupt or suspend processes.

## Requirements

C Compiler: GCC or Clang.
Unix-like System: Linux or macOS for compatibility with system calls and signal handling.

## Compilation

Run make, the makefile will create the executable file.

## Usage

Run the program using ./dragonshell

## Design choices:

Signal handling (uses sigaction, signal): 

I decided to implement signal handling as was shown in lecture 6: signals. It was explained that sigaction supercedes signal, so I followed close to the implementation that was shown there. This set up the signals for my handler so control z and control c don't stop and terminate the shell like they would do by default, as is shown in the assignment description. I also set up SIGCHLD so that any process sent to the background is able to dealt with once it is finished processing. Once it is finished, the SIGCHLD handler cleans it up. I also use the signal system call to restore default signal handling to the child processes in the function run_process. Each time a child is forked and the child is currently being processed, signal changes back SIGTSTP and SIGINT to their default state. I tested by presing control z and control c while in different scenarios including when a process is running in the foreground, in the background, and when there is no process running. This one gave me some trouble, and I still did not completely figure out SIGSTP, then calling SIGINT later.

Changing directory (uses chdir):

This seemed like a very staightforward implementation to me. I used chdir because it does exactly what we want it to do in the shell, and I just passed it the path. If the system call fails, it will simply tell the user there is no such file or directory. I tested this using all kinds of different paths as well as ../.

Print working directory (uses getcwd):

This is another very straightforward one. Used it because exactly what we want the shell to do. Gets the current path and prints it to the shell. Tested this by using cd command in shell then using pwd to see if it is properly printing the directory.

Running external programs (uses access, fork, execve, waitpid)

My run_process function become very large because I kept adding functionality to it but it started with just running other commands. I used access at the beginning of the function to check if the command that is trying to be ran can be found. I actually added this later into the implementation of this assignment. I was having trouble with the piping portion of the assignment and my program was parsing the paths before it even knew if it could find this file, which was causing other troubles having to do with piping, so I changed it to check at the beginning. Fork is also used in order to create a new process, we then check to see the current pid of the process in order to decide to do operations expected of the child or of the parent. The child process then uses execve in order to run the command that was entered into the shell. Figuring out the format of execve was difficult. It took me a while in order to realize that the name of the command we are trying to run needs to be the first element in the arguments given to execve. The parent process then uses waitpid to wait until it's child process is done. It also checks to see if the child was stopped by a signal, and if it was, it records its pid in a global variable to later remove when exit is called. I tested this using /bin/echo lots, as well as /bin/sleep with different numbers. I also tried many times running commands that did not exist within the directory.

Supporting background execution (uses waitpid)

So this one was tricky. I initially just thought that I could just skip the waitpid and not worry about the execution of this process and it would be fine. I later realized that I had to utilize a handler for my parent process in order to know when its child process was finished. So my SIGCHLD handler deals with pruning the child process when it is finished by calling waitpid.

Redirecting standard input/output (uses open, close, dup2)

This is also all contained in the run_process function because it was easiest to implement there. This function uses open to get the file descriptors for both the input file and output file given as arguments on the command line. Then for the input file, STDIN is redirected to the file that we are reading input from, and this is what dup2 is used to do. Similarly, for the output file, if it is used, STDOUT gets redirected to the descriptor of the output file using dup2. Then finally in both of these scenarios, close is used to close the file that was opened using the file descriptor. I tested the implementation of this by writing to lots of different .out files. Like a.out, b.out... I then would use /bin/cat to take the text in these files and output them either to the dragonshell or to new .out files.

Support piping (uses pipe, close, fork, waitpid, dup2, execve)

This is also all contained in the run_process function. I had to expand on this function in order to handle a second command, as well as arguments possibly given for a second command. When parsing, if it is found that there is a | contained in the arguments, then the function will call pipe(). Pipe will create file descriptors for both the left and right side of the pipe. It will then redirect STDOUT to the pipe's write end using dup2. It also close to close the unused end of the pipe, as well as the end that is used, after the file desc has been copied over. The parent process also uses fork again, in order to create the second process used in the pipe, and close, and dup2 are used similarly but for the read side of the pipe instead. execve is called on both processes for the pipe, and waitpid is called twice by the parent in order to wait for both processes to finish. This one was tricky to test, and I'm still unsure if it works 100% properly. I tested it on the example given in the assignment description and it seems to work as intended and sorts everything properly.

Exiting the terminal (uses times, kill)

The exit_dragonshell function is used to clean up unpruned processes, print the total processing time of child processes and exit the shell. times is used in order to grab the total time of all child processes. I was originally going to handle this by implementing a global variable and keeping track of time used before a forked process and after the process returns, but then I realized you can actually just get the user and system time from the children directly from the struct from times, so this is the approach that I took. Before doing this though, kill is used to terminate either stopped or processes currently running by sending a SIGTERM. I tested this using multiple prompts, having stopped processes, as well as currently running processes.
