1. In this assignment I suggested you use `fgets()` to get user input in the main while loop. Why is `fgets()` a good choice for this application?

    > **Answer**: `fgets()` is ideal because it:
    - Reads input line-by-line, matching shell behavior
    - Handles buffer overflow protection via size parameter
    - Returns NULL on EOF for clean program termination
    - Includes newline character for proper line processing
    - Works well with input redirection and pipes

2. You needed to use `malloc()` to allocate memory for `cmd_buff` in `dsh_cli.c`. Can you explain why you needed to do that, instead of allocating a fixed-size array?

    > **Answer**: `malloc()` is necessary because:
    - Shell runs indefinitely, making heap allocation more appropriate
    - Prevents stack overflow in long-running programs
    - Allows proper memory cleanup on exit
    - Dynamic allocation enables future buffer size changes
    - Provides error handling through allocation failure detection

3. In `dshlib.c`, the function `build_cmd_list()` must trim leading and trailing spaces from each command before storing it. Why is this necessary? If we didn't trim spaces, what kind of issues might arise when executing commands in our shell?

    > **Answer**: Space trimming is critical because:
    - Commands must execute consistently ("ls" = " ls ")
    - Pipe operations require accurate command boundaries
    - System calls expect clean command names
    - Argument parsing needs normalized spacing
    - Shell compatibility requires standardized formatting

4. For this question you need to do some research on STDIN, STDOUT, and STDERR in Linux. We've learned this week that shells are "robust brokers of input and output". Google _"linux shell stdin stdout stderr explained"_ to get started.

- One topic you should have found information on is "redirection". Please provide at least 3 redirection examples that we should implement in our custom shell, and explain what challenges we might have implementing them.

    > **Answer**: 
    1. Output redirection (>):
       - File creation/permissions handling
       - Atomic write operations
       - Append mode support (>>)
    
    2. Input redirection (<):
       - File existence verification
       - Read permission checking
       - Large file handling
    
    3. Error redirection (2>):
       - Separate file descriptor management
       - STDERR independence
       - Support for merging with STDOUT (2>&1)

- You should have also learned about "pipes". Redirection and piping both involve controlling input and output in the shell, but they serve different purposes. Explain the key differences between redirection and piping.

    > **Answer**: 
    - Pipes connect processes in memory, redirection connects with files
    - Pipes enable real-time data flow, redirection uses filesystem
    - Pipes are bidirectional (can send data both ways), redirection is unidirectional
    - Pipes handle backpressure automatically through buffering mechanisms
    - Pipes preserve data types and binary data, redirection converts to text
    - Pipes are temporary and exist only while processes run, redirection creates persistent files
    - Pipes connect multiple processes in a pipeline, redirection works with single process I/O
    - Pipes allow parallel execution of commands, redirection is sequential

- STDERR is often used for error messages, while STDOUT is for regular output. Why is it important to keep these separate in a shell?

    > **Answer**: 
    - Error visibility maintained during output redirection
    - Enables script error handling
    - Prevents error messages from corrupting program output
    - Supports logging and debugging
    - Allows automated error processing

- How should our custom shell handle errors from commands that fail? Consider cases where a command outputs both STDOUT and STDERR. Should we provide a way to merge them, and if so, how?

    > **Answer**: 
    - Display errors on STDERR by default
    - Provide exit codes for failed commands
    - Support 2>&1 for stream merging
    - Implement error propagation in pipes
    - Buffer errors to prevent output interleaving
    - Enable customizable error handling