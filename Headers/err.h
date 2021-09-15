#ifndef BLOCK3_ERR_H
#define BLOCK3_ERR_H

#include <stdio.h>
#include <stdlib.h>

#define PRINT_PEER_INFO 1

#ifndef __FILENAME__
    #include <libgen.h> // basename
    #define __FILENAME__ basename(__FILE__)
#endif  // __FILENAME__


#ifndef DEBUG
    #ifndef PRINT_DEBUG
        #define PRINT_DEBUG 0
    #endif // PRINT_DEBUG
    #ifndef PRINT_EXTRA_CRAP
        #define PRINT_EXTRA_CRAP 1
    #endif // PRINT_EXTRA_CRAP
    #define DEBUG(fmt, ...) \
            do { \
                if (PRINT_DEBUG) { \
                    if (PRINT_EXTRA_CRAP) { \
                        fprintf(stderr, "%s:%d:%s() DEBUG: " fmt, __FILENAME__, __LINE__, __func__, ##__VA_ARGS__); \
                    } else { \
                        fprintf(stderr, fmt, ##__VA_ARGS__); \
                    } \
                } \
            } while (0)
#endif //DEBUG

#ifndef ERROR
    #define ERROR(fmt, ...) \
            fprintf(stderr, "%s:%d:%s() ERROR: " fmt, __FILENAME__, __LINE__, __func__, ## __VA_ARGS__);
#endif  // ERROR

#ifndef ERROR_EXIT
    #define ERROR_EXIT(...)         \
        do {                        \
            ERROR(__VA_ARGS__);     \
            exit(EXIT_FAILURE);     \
        } while (0)
#endif // ERROR_EXIT

#endif // BLOCK3_ERR_H
