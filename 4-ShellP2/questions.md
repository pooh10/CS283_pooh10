1. Can you think of why we use `fork/execvp` instead of just calling `execvp` directly? What value do you think the `fork` provides?

    > **Answer**:  We use fork() before execvp() because execvp() completely replaces the current process with the new program. By forking first, we create a child process that can be replaced by execvp() while the parent process (our shell) continues running. This allows us to maintain the shell's state and continue accepting commands after the executed program finishes. In my implementation, this is demonstrated in exec_external_command() where I fork first, then only the child process calls execvp(), while the parent waits for the child to complete.

2. What happens if the fork() system call fails? How does your implementation handle this scenario?

    > **Answer**:  If fork() fails, it returns -1 and sets errno to indicate the error. In my implementation in exec_external_command(), I handle this by checking if pid < 0, then using perror("fork") to print the system error message, setting last_return_code to errno, and returning ERR_EXEC_CMD to indicate command execution failure. This ensures the shell continues running even if fork fails and provides appropriate error feedback to the user.

3. How does execvp() find the command to execute? What system environment variable plays a role in this process?

    > **Answer**:  execvp() searches for the command in the directories listed in the PATH environment variable. The 'p' in execvp() means it will use PATH to find the executable. In my implementation, I use execvp() instead of execv() because it handles PATH lookup automatically. This is why commands like "ls" or "pwd" work without specifying their full paths (/bin/ls or /bin/pwd), as execvp() finds them in the PATH directories.

4. What is the purpose of calling wait() in the parent process after forking? What would happen if we didn’t call it?

    > **Answer**:  In my implementation, I use waitpid() in the parent process to wait for the child process to complete. This serves two purposes: 1) It prevents zombie processes by properly collecting the child's exit status, and 2) It ensures sequential command execution by making the shell wait for each command to finish before accepting the next one. If we didn't call wait(), child processes would become zombies (defunct processes) and we wouldn't be able to get their exit status for the return code tracking.

5. In the referenced demo code we used WEXITSTATUS(). What information does this provide, and why is it important?

    > **Answer**:  WEXITSTATUS() extracts the exit status from the status value returned by waitpid(). In my implementation, I use it in exec_external_command() to get the actual return code from the child process: last_return_code = WEXITSTATUS(status). This is important for implementing the return code tracking (rc command) and error handling. For example, when a command fails with "command not found", WEXITSTATUS() lets us capture the ENOENT (2) error code.

6. Describe how your implementation of build_cmd_buff() handles quoted arguments. Why is this necessary?

    > **Answer**:  My build_cmd_buff() implementation uses a boolean flag in_quotes to track whether we're inside quoted text. When a quote is encountered, it sets in_quotes true and starts capturing all characters (including spaces) until the closing quote. This is necessary to handle arguments with spaces like echo "hello   world", where the spaces inside quotes should be preserved as a single argument. Without this, the argument would be incorrectly split into multiple arguments at each space.

7. What changes did you make to your parsing logic compared to the previous assignment? Were there any unexpected challenges in refactoring your old code?

    > **Answer**:  The main change was simplifying the parsing logic to handle single commands instead of pipes. The cmd_buff_t structure is simpler than the previous command_list_t as it only needs to handle one command at a time. The main challenge was ensuring proper memory management with strdup() for argument copying and corresponding free() calls in clear_cmd_buff(). I also had to make sure the parsing preserved spaces in quoted strings while still trimming unquoted spaces correctly.

8. For this quesiton, you need to do some research on Linux signals. You can use [this google search](https://www.google.com/search?q=Linux+signals+overview+site%3Aman7.org+OR+site%3Alinux.die.net+OR+site%3Atldp.org&oq=Linux+signals+overview+site%3Aman7.org+OR+site%3Alinux.die.net+OR+site%3Atldp.org&gs_lcrp=EgZjaHJvbWUyBggAEEUYOdIBBzc2MGowajeoAgCwAgA&sourceid=chrome&ie=UTF-8) to get started.

- What is the purpose of signals in a Linux system, and how do they differ from other forms of interprocess communication (IPC)?

    > **Answer**:  Signals are software interrupts used to notify processes of events. Unlike other IPC methods (pipes, sockets, shared memory), signals are asynchronous and don't carry data beyond the signal number. They're used for simple notifications like process termination (SIGTERM), interrupt requests (SIGINT), or child process status changes (SIGCHLD).

- Find and describe three commonly used signals (e.g., SIGKILL, SIGTERM, SIGINT). What are their typical use cases?

    > **Answer**:  1) SIGINT (2): Sent when user presses Ctrl+C, used for interactive program interruption. 2) SIGTERM (15): Standard signal for requesting program termination, allows cleanup. 3) SIGKILL (9): Forces immediate process termination, cannot be caught or ignored.

- What happens when a process receives SIGSTOP? Can it be caught or ignored like SIGINT? Why or why not?

    > **Answer**:  SIGSTOP suspends process execution and cannot be caught or ignored, unlike SIGINT. This is by design to ensure the operating system can always control process execution. It's paired with SIGCONT which resumes execution.
