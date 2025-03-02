#!/usr/bin/env bats

# File: student_tests.sh
# 
# Create your unit tests suit in this file

@test "Basic command execution" {
    run "./dsh" <<EOF
ls
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "exiting..." ]]
}

@test "Basic pipe functionality" {
    run "./dsh" <<EOF
echo "hello world" | grep "hello"
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "hello world" ]]
    [[ "$output" =~ "exiting..." ]]
}

@test "Multiple pipes" {
    run "./dsh" <<EOF
echo "line1\nline2\nline3" | grep "line" | wc -l
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "3" ]]
    [[ "$output" =~ "exiting..." ]]
}

@test "Handle quoted arguments in pipes" {
    run "./dsh" <<EOF
echo "hello   world" | grep "hello"
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "hello   world" ]]
    [[ "$output" =~ "exiting..." ]]
}

@test "Empty command handling" {
    run "./dsh" <<EOF

exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "warning: no commands provided" ]]
    [[ "$output" =~ "exiting..." ]]
}

@test "CD command works correctly" {
    current=$(pwd)
    run "./dsh" <<EOF
cd /tmp
pwd
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "/tmp" ]]
    [[ "$output" =~ "exiting..." ]]
}

@test "CD command with no args doesn't change directory" {
    current=$(pwd)
    run "./dsh" <<EOF
cd
pwd
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "$current" ]]
    [[ "$output" =~ "exiting..." ]]
}

@test "Too many piped commands" {
    run "./dsh" <<EOF
echo "test" | grep "test" | grep "test" | grep "test" | grep "test" | grep "test" | grep "test" | grep "test" | grep "test"
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "error: piping limited to 8 commands" ]]
    [[ "$output" =~ "exiting..." ]]
}

@test "Command not found error handling" {
    run "./dsh" <<EOF
nonexistentcommand
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "Command not found in PATH" ]]
    [[ "$output" =~ "exiting..." ]]
}

@test "Pipes with complex commands" {
    run "./dsh" <<EOF
ls -la | grep "\\." | sort -r | head -n 3
exit
EOF
    [ "$status" -eq 0 ]
    # We don't check specific output since it depends on directory content
    # Just check that the command ran without errors
    [[ "$output" =~ "exiting..." ]]
}






@test "Basic output redirection" {
    run "./dsh" <<EOF
echo "hello redirection" > test_out.txt
cat test_out.txt
rm test_out.txt
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "hello redirection" ]]
    [[ "$output" =~ "exiting..." ]]
}

@test "Append output redirection" {
    run "./dsh" <<EOF
echo "line 1" > test_append.txt
echo "line 2" >> test_append.txt
cat test_append.txt
rm test_append.txt
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "line 1" ]]
    [[ "$output" =~ "line 2" ]]
    [[ "$output" =~ "exiting..." ]]
}

@test "Input redirection" {
    run "./dsh" <<EOF
echo "test input redirection" > test_in.txt
cat < test_in.txt
rm test_in.txt
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "test input redirection" ]]
    [[ "$output" =~ "exiting..." ]]
}