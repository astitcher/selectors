// C Interface to selector library

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct selector_expression_t selector_expression_t;
typedef struct selector_value_t selector_value_t;
typedef struct selector_environment_t selector_environment_t;

struct selector_environment_t {
    void (*free)();
    const selector_value_t* (*value)(const char* id);
};

__attribute__((visibility("default"))) const selector_expression_t* selector_expression(const char* exp);
__attribute__((visibility("default"))) void selector_expression_free(const char* exp);
__attribute__((visibility("default"))) bool selector_expression_eval(const selector_expression_t* exp, const selector_environment_t* env);
__attribute__((visibility("default"))) void selector_expression_dump(const selector_expression_t* exp);

__attribute__((visibility("default"))) const selector_value_t* selector_value_unknown();
__attribute__((visibility("default"))) const selector_value_t* selector_value_bool(bool b);
__attribute__((visibility("default"))) const selector_value_t* selector_value_exact(int64_t i);
__attribute__((visibility("default"))) const selector_value_t* selector_value_approx(double d);
__attribute__((visibility("default"))) const selector_value_t* selector_value_string(const char* str);
__attribute__((visibility("default"))) void selector_value_free(const selector_value_t* v);
__attribute__((visibility("default"))) void selector_value_dump(const selector_value_t* v);

#ifdef __cplusplus
};
#endif
