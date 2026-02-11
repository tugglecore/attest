#include "attest.h"

BEFORE_ALL(ctx)
{
    int* foo = malloc(sizeof(int));
    *foo = 3;
    ctx->all = (void*)foo;
}

AFTER_ALL(ctx)
{
    free(ctx->all);
}

PARAM_TEST(candy_basket,
    int,
    num,
    (
        {  "one name", .data = 3, },
        {.name = "two name", .data = 3 },
        { "", .data = 3 },
        { .name = "fourth name", .data = 4 },
        { .data = 5 },
        { "", 7 }))
{
    EXPECT_EQ(num, 1, "not a one");
    EXPECT_EQ(num, 3, "not a three");
}

void before_all_cases(ParamContext* context)
{
    int* foo = malloc(sizeof(int));
    *foo = 7;
    context->set = (void*)foo;
}

void after_all_cases(ParamContext* context)
{
    free(context->set);
}

void before_each_case(ParamContext* context)
{
    int* foo = malloc(sizeof(int));
    *foo = 10;
    context->self = (void*)foo;
}

void after_each_case(ParamContext* context)
{
    free(context->self);
}

PARAM_TEST_CTX(basket_case,
    context,
    int,
    num,
    ({ .data = 1 }, { .data = 2 }, { .data = 3 }),
    .before_all_cases = before_all_cases,
    .before_each_case = before_each_case,
    .after_all_cases = after_all_cases,
    .after_each_case = after_each_case, )
{
    int shared_num = *(int*)context->all;
    int set_num = *(int*)context->set;
    int local_num = *(int*)context->self;
    EXPECT_EQ(shared_num + set_num + local_num + num, 1);
}
