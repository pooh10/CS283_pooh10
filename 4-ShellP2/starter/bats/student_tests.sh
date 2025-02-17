#!/usr/bin/env bats

bats_test_function --description Execute\ basic\ external\ command  --tags "" -- test_Execute_basic_external_command;test_Execute_basic_external_command() { 
    run "./dsh" <<EOF
ls
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "dsh2>" ]]
}

bats_test_function --description Execute\ command\ with\ arguments  --tags "" -- test_Execute_command_with_arguments;test_Execute_command_with_arguments() { 
    run "./dsh" <<EOF
echo test argument
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "test argument" ]]
}

bats_test_function --description Handle\ empty\ command  --tags "" -- test_Handle_empty_command;test_Handle_empty_command() { 
    run "./dsh" <<EOF

exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "warning: no commands provided" ]]
}

bats_test_function --description CD\ command\ -\ no\ arguments  --tags "" -- test_CD_command_-2d_no_arguments;test_CD_command_-2d_no_arguments() { 
    current=$(pwd)
    run "./dsh" <<EOF
cd
pwd
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "$current" ]]
}

bats_test_function --description CD\ command\ -\ with\ argument  --tags "" -- test_CD_command_-2d_with_argument;test_CD_command_-2d_with_argument() { 
    run "./dsh" <<EOF
cd /tmp
pwd
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "/tmp" ]]
}

bats_test_function --description Handle\ quoted\ arguments\ with\ spaces  --tags "" -- test_Handle_quoted_arguments_with_spaces;test_Handle_quoted_arguments_with_spaces() { 
    run "./dsh" <<EOF
echo "hello   world   test"
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "hello   world   test" ]]
}

# Extra Credit Tests

bats_test_function --description Command\ not\ found\ error\ handling  --tags "" -- test_Command_not_found_error_handling;test_Command_not_found_error_handling() { 
    run "./dsh" <<EOF
nonexistentcommand
rc
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "Command not found in PATH" ]]
    [[ "$output" =~ "2" ]]  # ENOENT is usually 2
}

bats_test_function --description Permission\ denied\ error\ handling  --tags "" -- test_Permission_denied_error_handling;test_Permission_denied_error_handling() { 
    run "./dsh" <<EOF
touch testfile
chmod 000 testfile
./testfile
rc
rm -f testfile
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "Permission denied" ]]
    [[ "$output" =~ "13" ]]  # EACCES is usually 13
}

bats_test_function --description Return\ code\ tracking\ with\ successful\ command  --tags "" -- test_Return_code_tracking_with_successful_command;test_Return_code_tracking_with_successful_command() { 
    run "./dsh" <<EOF
true
rc
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "0" ]]
}

bats_test_function --description Return\ code\ tracking\ with\ failing\ command  --tags "" -- test_Return_code_tracking_with_failing_command;test_Return_code_tracking_with_failing_command() { 
    run "./dsh" <<EOF
false
rc
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "1" ]]
}

bats_test_function --description CD\ to\ nonexistent\ directory\ error\ handling  --tags "" -- test_CD_to_nonexistent_directory_error_handling;test_CD_to_nonexistent_directory_error_handling() { 
    run "./dsh" <<EOF
cd /nonexistent/directory
rc
exit
EOF
    [ "$status" -eq 0 ]
    [[ "$output" =~ "cd:" ]]
    [[ "$output" =~ "2" ]]  # ENOENT
}
