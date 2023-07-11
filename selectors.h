#ifndef SELECTORS_H
#define SELECTORS_H
// C Interface to selector library

#include <stdbool.h>
#include <stdint.h>

#include "selectors_export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct selector_expression_t selector_expression_t;
typedef struct selector_value_t selector_value_t;
typedef struct selector_environment_t selector_environment_t;

SELECTORS_EXPORT const selector_expression_t* selector_expression(const char* exp);
SELECTORS_EXPORT void selector_expression_free(const selector_expression_t* exp);
SELECTORS_EXPORT bool selector_expression_eval(const selector_expression_t* exp, const selector_environment_t* env);
SELECTORS_EXPORT const selector_value_t* selector_expression_value(const selector_expression_t* exp, const selector_environment_t* env);
SELECTORS_EXPORT void selector_expression_dump(const selector_expression_t* exp);

SELECTORS_EXPORT const selector_value_t* selector_value(const char* str);
SELECTORS_EXPORT const selector_value_t* selector_value_unknown();
SELECTORS_EXPORT const selector_value_t* selector_value_bool(bool b);
SELECTORS_EXPORT const selector_value_t* selector_value_exact(int64_t i);
SELECTORS_EXPORT const selector_value_t* selector_value_approx(double d);
SELECTORS_EXPORT const selector_value_t* selector_value_string(const char* str);
SELECTORS_EXPORT void selector_value_free(const selector_value_t* v);
SELECTORS_EXPORT void selector_value_dump(const selector_value_t* v);

SELECTORS_EXPORT selector_environment_t* selector_environment();
SELECTORS_EXPORT void selector_environment_free(const selector_environment_t* env);
SELECTORS_EXPORT void selector_environment_dump(const selector_environment_t* env);
SELECTORS_EXPORT const selector_value_t* selector_environment_get(const selector_environment_t* env, const char *var);
SELECTORS_EXPORT void selector_environment_set(selector_environment_t* env, const char *var, const selector_value_t* val);

SELECTORS_EXPORT const char* selector_intern(const char* exp);

#ifdef __cplusplus
};
#endif

#endif
