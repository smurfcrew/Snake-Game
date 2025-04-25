#include "encrypt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// basic XOR encrytion function
bool encrypt_file(const char* input_file_path, const char* output_file_path, const char* password) {
    FILE* input_file = fopen(input_file_path, "r");
    if (!input_file) {
        perror("Error opening input file");
        return false;
    }
    
    FILE* output_file = fopen(output_file_path, "wb");
    if (!output_file) {
        perror("Error opening output file");
        fclose(input_file);
        return false;
    }
    
    unsigned int salt = (unsigned int)time(NULL);
    fwrite(&salt, sizeof(salt), 1, output_file);
    
    size_t password_len = strlen(password);
    if (password_len == 0) {
        fprintf(stderr, "Error: Empty password\n");
        fclose(input_file);
        fclose(output_file);
        return false;
    }
    
    
    int ch;
    size_t i = 0;
    
    while ((ch = fgetc(input_file)) != EOF) {
        
        unsigned char encrypted_char = ch ^ password[i] ^ ((salt >> (i % 32)) & 0xFF);
        fputc(encrypted_char, output_file);
        i = (i + 1) % password_len;
    }
    
    fclose(input_file);
    fclose(output_file);
    printf("File encrypted successfully: %s -> %s\n", input_file_path, output_file_path);
    return true;
}