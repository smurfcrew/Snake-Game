#ifndef ENCRYPT_H
#define ENCRYPT_H

#include <stdio.h>
#include <stdbool.h>

bool encrypt_file(const char* input_file_path, const char* output_file_path, const char* password);

#endif // ENCRYPT_H