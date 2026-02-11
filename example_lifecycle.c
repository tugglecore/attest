#include "attest.h"

BEFORE_ALL(context)
{
    int* foo = malloc(sizeof(int));
    if (!foo) {
        exit(1);
    }
    *foo = 7;
    context->all = (void*)foo;
}

AFTER_ALL(context)
{
    free(context->all);
    context->all = NULL;
}

BEFORE_EACH(context)
{
    int* bar = malloc(sizeof(int));

    if (!bar) {
        exit(1);
    }

    *bar = 4;
    context->each = (void*)bar;
}

AFTER_EACH(context)
{
    free(context->each);
    context->each = NULL;
}

// Lets move onto something completely different: Lifecycle methods.
// A given test can have a before which runs some code before the
// test function and an after function which runs some code after the
// test function. Additionally, the before, test and after function
// will be passed a Context object which has a void* data field which
// can be used to share information between the three functions.
void setup(TestContext* context)
{
    int* random_number = malloc(sizeof(int));

    if (!random_number) {
        exit(1);
    }

    *random_number = 10;
    context->self = (void*)random_number;
}

void cleanup(TestContext* context)
{
    free(context->self);
    context->self = NULL;
}

TEST_CTX(with_a_context, context, .before = setup, .after = cleanup)
{
    int global_num = *(int*)context->all;
    int each_num = *(int*)context->each;
    int self_num = *(int*)context->self;

    EXPECT_EQ(global_num + each_num + self_num, 21);
}
