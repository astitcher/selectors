#include "selectors.h"

#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

void process(const char* str, selector_environment_t* env)
{
    // Check for special commands
    if (*str=='\\'){
        ++str;
        switch (*str) {
          case 'b':
            // Binary
            break;
          case 'x':
          case 'h':
            break;
          default:
            printf("Unrecognized special command: %c\n", *str);
            return;
        }
        ++str;
    }

    const selector_expression_t* exp = selector_expression(str);
    if (!exp) return;

    selector_expression_dump(exp);
    printf("\n");

    const selector_value_t* v = selector_expression_value(exp, env);
    selector_value_dump(v);
    printf("\n");

    selector_value_free(v);
    selector_expression_free(exp);
}

int main(int argc, const char* argv[])
{
  selector_environment_t* env = selector_environment();
  for (int i=1; i<argc; i+=2) {
    selector_environment_set(env, argv[i], selector_value_string(argv[i+1]));
  }

  selector_environment_dump(env);

  char* buffer = readline(">> ");
  while (buffer) {
    add_history(buffer);
    process(buffer, env);
    free(buffer);
    buffer = readline(">> ");
  }
  free(buffer);

  selector_environment_free(env);
  return 0;
}
