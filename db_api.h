#ifndef DBAPI_H
#define DBAPI_H
#ifdef __cplusplus
extern "C"{
#endif

int delete_program(char* program);
int add_program(char  *program, char *extension);
char **select_program(char* file);
int run(char* program, char* file);

#ifdef __cplusplus 
}
#endif
#endif