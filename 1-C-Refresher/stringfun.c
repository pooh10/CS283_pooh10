/**
@file
@author Pujan Pokharel
@date January 17, 2024
@section DESCRIPTION
A string processing utility that provides various operations like word counting,
string reversal, word printing and string replacement.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SZ 50

//prototypes
void usage(char *);
void print_buff(char *, int);
int  setup_buff(char *, char *, int);

//prototypes for functions to handle required functionality 
int count_words(char *, int, int);
//add additional prototypes here
int reverse_string(char *, int, int);
int print_words(char *, int, int);
int replace_string(char *, int, int, char *, char *);

int setup_buff(char *buff, char *user_str, int len) {
    //TODO: #4:  Implement the setup buff as per the directions
    if (!buff || !user_str || len <= 0) return -2; // Invalid parameters

    int i = 0;          // Index for input string
    int j = 0;          // Index for buffer
    int space_flag = 1;
    
    // Processing the input string
    while (user_str[i] != '\0') {
        // Skip duplicate whitespace
        if (user_str[i] == ' ' || user_str[i] == '\t') {
            if (!space_flag) {
                if (j >= len) return -1;
                buff[j++] = ' ';
                space_flag = 1;
            }
        } else {
            if (j >= len) return -1;
            buff[j++] = user_str[i];
            space_flag = 0;
        }
        i++;
    }

    // Remove trailing space if exists
    if (j > 0 && buff[j-1] == ' ') {
        j--;
    }

    int str_len = j; 

    // Fill remainder with dots
    while (j < len) {
        buff[j++] = '.';
    }

    return str_len;
}

void print_buff(char *buff, int len) {
    printf("Buffer:  [");
    for (int i = 0; i < len; i++) {
        putchar(*(buff+i));
    }
    putchar(']');
    putchar('\n');
}

void usage(char *exename) {
    printf("usage: %s [-h|c|r|w|x] \"string\" [other args]\n", exename);
}

int count_words(char *buff, int len, int str_len) {
    //YOU MUST IMPLEMENT
    if (!buff || len <= 0 || str_len <= 0 || str_len > len) return -1;

    int word_count = 0;
    int in_word = 0;

    for (int i = 0; i < str_len; i++) {
        if (*(buff + i) != ' ' && !in_word) {
            word_count++;
            in_word = 1;
        } else if (*(buff + i) == ' ') {
            in_word = 0;
        }
    }

    return word_count;
}

//ADD OTHER HELPER FUNCTIONS HERE FOR OTHER REQUIRED PROGRAM OPTIONS
int reverse_string(char *buff, int len, int str_len) {
    if (!buff || len <= 0 || str_len <= 0 || str_len > len) return -1;

    char temp;
    for (int i = 0; i < str_len / 2; i++) {
        temp = *(buff + i);
        *(buff + i) = *(buff + str_len - 1 - i);
        *(buff + str_len - 1 - i) = temp;
    }

    return 0;
}

int print_words(char *buff, int len, int str_len) {
    if (!buff || len <= 0 || str_len <= 0 || str_len > len) return -1;

    int word_count = 0;
    int char_count = 0;
    int at_start = 1;

    printf("Word Print\n----------\n");

    for (int i = 0; i < str_len; i++) {
        if (at_start && *(buff + i) != ' ') {
            word_count++;
            printf("%d. ", word_count);
            at_start = 0;
        }

        if (*(buff + i) == ' ') {
            if (!at_start) {
                printf("(%d)\n", char_count);
                char_count = 0;
            }
            at_start = 1;
        } else {
            putchar(*(buff + i));
            char_count++;
        }
    }

    if (char_count > 0) {
        printf("(%d)\n", char_count);
    }

    printf("\nNumber of words returned: %d\n", word_count);
    return word_count;
}

int replace_string(char *buff, int len, int str_len, char *search, char *replace) {
    if (!buff || !search || !replace || len <= 0 || str_len <= 0) return -1;

    int search_len = 0;
    int replace_len = 0;

    // Get lengths without using strlen
    while (search[search_len] != '\0') search_len++;
    while (replace[replace_len] != '\0') replace_len++;

    // Find the search string
    for (int i = 0; i <= str_len - search_len; i++) {
        int found = 1;
        for (int j = 0; j < search_len; j++) {
            if (*(buff + i + j) != search[j]) {
                found = 0;
                break;
            }
        }
        
        if (found) {
            // Calculate new string length after replacement
            int new_len = str_len - search_len + replace_len;
            if (new_len > len) {
                // If replacement would exceed buffer, truncate
                new_len = len;
            }

            // Shift the rest of the string if needed
            if (replace_len != search_len) {
                int shift = replace_len - search_len;
                if (shift < 0) {
                    // Replacement is shorter
                    for (int k = i + replace_len; k < new_len; k++) {
                        *(buff + k) = *(buff + k - shift);
                    }
                } else {
                    // Replacement is longer
                    for (int k = new_len - 1; k >= i + replace_len; k--) {
                        *(buff + k) = *(buff + k - shift);
                    }
                }
            }

            // Copy replacement string
            for (int k = 0; k < replace_len && (i + k) < len; k++) {
                *(buff + i + k) = replace[k];
            }

            // Fill remainder with dots if needed
            for (int k = new_len; k < len; k++) {
                *(buff + k) = '.';
            }

            return 0;
        }
    }

    return -1; // Search string not found
}

int main(int argc, char *argv[]) {
    char *buff = NULL;        //placeholder for the internal buffer
    char *input_string;       //holds the string provided by the user on cmd line
    char opt;                 //used to capture user option from cmd line
    int  rc;                  //used for return codes
    int  user_str_len;        //length of user supplied string

    //TODO:  #1. WHY IS THIS SAFE, aka what if arv[1] does not exist?
    //      PLACE A COMMENT BLOCK HERE EXPLAINING

    /* TODO #1: This is safe because we check both:
     * 1. If argc < 2 (ensures argv[1] exists)
     * 2. If first char of argv[1] is '-'
     * If either check fails, we show usage and exit
     */
    if ((argc < 2) || (*argv[1] != '-')) {
        usage(argv[0]);
        exit(1);
    }

    opt = (char)*(argv[1]+1);   //get the option flag

    //handle the help flag and then exit normally
    if (opt == 'h') {
        usage(argv[0]);
        exit(0);
    }

    //WE NOW WILL HANDLE THE REQUIRED OPERATIONS

    //TODO:  #2 Document the purpose of the if statement below
    //      PLACE A COMMENT BLOCK HERE EXPLAINING

    /* TODO #2: This checks if we have enough arguments for all operations
     * All operations except -h require at least a string argument
     * So we need at least 3 arguments: program name, option, and string
     */
    if (argc < 3) {
        usage(argv[0]);
        exit(1);
    }

    input_string = argv[2]; //capture the user input string

    //TODO:  #3 Allocate space for the buffer using malloc and
    //          handle error if malloc fails by exiting with a 
    //          return code of 99
    // CODE GOES HERE FOR #3

    // Allocate buffer memory
    buff = (char *)malloc(BUFFER_SZ);
    if (!buff) {
        printf("Error: Failed to allocate memory\n");
        exit(99);
    }

    user_str_len = setup_buff(buff, input_string, BUFFER_SZ);
    if (user_str_len < 0) {
        printf("Error setting up buffer, error = %d\n", user_str_len);
        free(buff);
        exit(2);
    }

    switch (opt) {
        case 'c':
            rc = count_words(buff, BUFFER_SZ, user_str_len);
            if (rc < 0) {
                printf("Error counting words, rc = %d\n", rc);
                free(buff);
                exit(2);
            }
            printf("Word Count: %d\n", rc);
            break;

        //TODO:  #5 Implement the other cases for 'r' and 'w' by extending
        //       the case statement options

        case 'r':
            rc = reverse_string(buff, BUFFER_SZ, user_str_len);
            if (rc < 0) {
                printf("Error reversing string, rc = %d\n", rc);
                free(buff);
                exit(2);
            }
            break;

        case 'w':
            rc = print_words(buff, BUFFER_SZ, user_str_len);
            if (rc < 0) {
                printf("Error printing words, rc = %d\n", rc);
                free(buff);
                exit(2);
            }
            break;

        case 'x':
            if (argc < 5) {
                printf("Error: -x option requires search and replace strings\n");
                free(buff);
                exit(1);
            }
            rc = replace_string(buff, BUFFER_SZ, user_str_len, argv[3], argv[4]);
            if (rc < 0) {
                printf("Error: Search string not found\n");
                free(buff);
                exit(3);
            }
            break;

        default:
            usage(argv[0]);
            free(buff);
            exit(1);
    }

    //TODO:  #6 Dont forget to free your buffer before exiting
    print_buff(buff, BUFFER_SZ);
    free(buff);
    exit(0);
}

//TODO:  #7  Notice all of the helper functions provided in the 
//          starter take both the buffer as well as the length.  Why
//          do you think providing both the pointer and the length
//          is a good practice, after all we know from main() that 
//          the buff variable will have exactly 50 bytes?
//  
//          PLACE YOUR ANSWER HERE

/* TODO #7: Providing both pointer and length is good practice because:
 * 1. Prevents buffer overflows by explicitly knowing boundaries
 * 2. Allows functions to work with different buffer sizes
 * 3. Functions can validate inputs before processing
 * 4. Makes it clear what the function's limits are
 * 5. Doesn't assume null-termination like string functions
 */