#include "attest.h"

BEFORE_ALL(ctx)
{
    int* foo = malloc(sizeof(int));
    *foo = 3;
    ctx->shared = (void*)foo;
}

PARAM_TEST(fruit_basket,
    int,
    num,
    (1, 2, 3))
{
    EXPECT_EQ(num, 1);
}

void before_all_cases(ParamContext* context)
{
    int* foo = malloc(sizeof(int));
    *foo = 7;
    context->set = (void*)foo;
}

void before_each_case(ParamContext* context)
{
    int* foo = malloc(sizeof(int));
    *foo = 10;
    context->local = (void*)foo;
}

void after_each_case(ParamContext* context)
{
    free(context->local);
}

void after_all_cases(ParamContext* context)
{
    free(context->set);
}

PARAM_TEST_CTX(basket_case,
    context,
    int,
    num,
    (1, 2, 3),
    .before_all_cases = before_all_cases,
    .before_each_case = before_each_case,
    .after_all_cases = after_all_cases,
    .after_each_case = after_each_case, )
{
    int shared_num = *(int*)context->shared;
    int set_num = *(int*)context->set;
    int local_num = *(int*)context->local;
    EXPECT_EQ(shared_num + set_num + local_num + num, 1);
}
