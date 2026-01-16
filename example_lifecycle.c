#include "attest.h"

BEFORE_ALL(context)
{
    int* foo = malloc(sizeof(int));
    *foo = 7;
    context->shared = (void*)foo;
}

BEFORE_EACH(context)
{
    int* foo = malloc(sizeof(int));
    *foo = *(int*)context->shared - 3;
    context->local = (void*)foo;
}

AFTER_ALL(context)
{
    free(context->shared);
}

AFTER_EACH(context)
{
    free(context->local);
    context->local = NULL;
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
    *random_number = *(int*)context->local + *(int*)context->shared + 3;
    context->local = (void*)random_number;
}

void cleanup(TestContext* context)
{
    free(context->local);
    context->local = NULL;
}

TEST_CTX(with_a_context, context, .before = setup, .after = cleanup)
{
    int global_num = *(int*)context->shared;
    int local_num = *(int*)context->local;

    EXPECT_EQ(global_num + local_num, 21);
}
