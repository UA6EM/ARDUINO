
#include <stdlib.h>

#include <gtest/gtest.h>

#include "Arduino.h"
#include "mock.h"

#include "../cli.h"
#include "../motor.h"

typedef struct {
    char cmd;
    int value;
    void (*fn)(char cmd, int value, void *arg);
}   CliArg;

static void on_a(char cmd, int value, void *arg)
{
    ASSERT(arg);
    CliArg *cli_arg = (CliArg*) arg;

    cli_arg->cmd = cmd;
    cli_arg->value = value;
    cli_arg->fn = on_a;
}

static void on_b(char cmd, int value, void *arg)
{
    ASSERT(arg);
    CliArg *cli_arg = (CliArg*) arg;

    cli_arg->cmd = cmd;
    cli_arg->value = value;
    cli_arg->fn = on_b;
}

    /*
     *
     */

TEST(Cli, Cli)
{
    CLI cli;
    CliArg arg;

    Action a = { 'A', on_a, & arg, 0 };
    cli.add_action(& a);

    // check A
    memset(& arg, 0, sizeof(arg));
    cli.process("A1234\r\n");
    EXPECT_EQ(arg.cmd, 'A');
    EXPECT_EQ(arg.value, 1234);
    EXPECT_EQ(arg.fn, on_a);

    Action b = { 'B', on_b, & arg, 0 };
    cli.add_action(& b);

    // check B
    memset(& arg, 0, sizeof(arg));
    cli.process("B0\r\n");
    EXPECT_EQ(arg.cmd, 'B');
    EXPECT_EQ(arg.value, 0);
    EXPECT_EQ(arg.fn, on_b);

    // check A still works
    memset(& arg, 0, sizeof(arg));
    cli.process("A1234\r\n");
    EXPECT_EQ(arg.cmd, 'A');
    EXPECT_EQ(arg.value, 1234);
    EXPECT_EQ(arg.fn, on_a);

    // check some non commands
    memset(& arg, 0, sizeof(arg));
    cli.process("123\r\n");
    EXPECT_EQ(arg.cmd, '\0');
    EXPECT_EQ(arg.value, 0);
    EXPECT_EQ(arg.fn, (void*) 0);

    // check no command
    memset(& arg, 0, sizeof(arg));
    cli.process("\r\n");
    EXPECT_EQ(arg.cmd, '\0');
    EXPECT_EQ(arg.value, 0);
    EXPECT_EQ(arg.fn, (void*) 0);

    // check default 0
    memset(& arg, 0, sizeof(arg));
    cli.process("A\r\n");
    EXPECT_EQ(arg.cmd, 'A');
    EXPECT_EQ(arg.value, 0);
    EXPECT_EQ(arg.fn, on_a);

    // check <backspace> deletes (reset)
    // actually, any unknown char will reset the line
    memset(& arg, 0, sizeof(arg));
    cli.process("A\b\r\n");
    EXPECT_EQ(arg.cmd, '\0');
    EXPECT_EQ(arg.value, 0);
    EXPECT_EQ(arg.fn, (void*)0);
}

    /*
     *
     */

static void seek_test(Stepper *stepper, int seek)
{
    stepper->seek(seek);
    const int start = stepper->position();
    int step = 1;
    if (seek < start)
    {
        step = -1;
    }

    for (int i = start; i != seek; i += step)
    {
        EXPECT_EQ(stepper->position(), i);
        EXPECT_EQ(stepper->ready(), false);
        // step to the next position
        stepper->poll();
    }

    // should now be in position
    EXPECT_EQ(stepper->position(), seek);
    EXPECT_EQ(stepper->ready(), true);
}

    /*
     *
     */

static void move(Stepper *stepper, int expect, bool *zero=0)
{
    if (zero)
    {
        *zero = false;
    }

    while (true)
    {
        const int pos = stepper->position();
        if (zero && (pos == 0))
        {
            // passed through zero
            *zero = true;
        }
        if (pos == expect)
            break;
        stepper->poll();
    }
}

static void move_to(Stepper *stepper, int t)
{
    const int steps = stepper->get_steps();
    int expect = t;

    // clip to valid range
    if (t < 0)
        expect = 0;
    if (t >= steps)
        expect = steps-1;

    stepper->seek(t);
    move(stepper, expect);
}

    /*
     *
     */

TEST(Motor, Seek)
{
    mock_setup();

    int cycle = 5000;
    Stepper stepper(cycle, 1, 2, 3, 4);

    // seek test
    EXPECT_EQ(stepper.position(), 0);
    seek_test(& stepper, 100);
    EXPECT_EQ(stepper.get_target(), 100);
    seek_test(& stepper, 0);
    seek_test(& stepper, 4999);
    seek_test(& stepper, 50);
    seek_test(& stepper, 1050);
    EXPECT_EQ(stepper.get_target(), 1050);

    // zero test
    stepper.zero();
    EXPECT_EQ(stepper.position(), 0);
    seek_test(& stepper, 100);

    // seek should clip at limits (<0)
    seek_test(& stepper, 100);
    stepper.seek(-1);
    while (!stepper.ready())
    {
        stepper.poll();
    }
    EXPECT_EQ(stepper.position(), 0);

    // seek should clip at limits (>= cycle)
    stepper.seek(cycle + 100);
    while (!stepper.ready())
    {
        stepper.poll();
    }
    EXPECT_EQ(stepper.position(), cycle - 1);

    // seek should clip at limits (>= cycle)
    stepper.seek(cycle);
    while (!stepper.ready())
    {
        printf("%d\n", stepper.position());
        stepper.poll();
    }
    EXPECT_EQ(stepper.position(), cycle - 1);

    mock_teardown();
}

    /*
     *
     */

TEST(Motor, Nowhere)
{
    mock_setup();

    int cycle = 5000;
    Stepper stepper(cycle, 1, 2, 3, 4);

    // seek test
    EXPECT_EQ(stepper.position(), 0);

    // check it goes nowhere
    stepper.seek(0);
    stepper.poll();
    EXPECT_EQ(stepper.position(), 0);
    EXPECT_TRUE(stepper.ready());

    move_to(& stepper, 100);
    EXPECT_EQ(stepper.position(), 100);

    // should already be there
    stepper.seek(100);
    stepper.poll();
    EXPECT_EQ(stepper.position(), 100);
    EXPECT_TRUE(stepper.ready());

    mock_teardown();
}

TEST(Motor, RotateNowhere)
{
    mock_setup();

    int cycle = 5000;
    Stepper stepper(cycle, 1, 2, 3, 4);

    // seek test
    EXPECT_EQ(stepper.position(), 0);

    // check it goes nowhere
    stepper.rotate(0);
    stepper.poll();
    EXPECT_EQ(stepper.position(), 0);
    EXPECT_TRUE(stepper.ready());

    move_to(& stepper, 100);
    EXPECT_EQ(stepper.position(), 100);

    // should already be there
    stepper.rotate(100);
    stepper.poll();
    EXPECT_EQ(stepper.position(), 100);
    EXPECT_TRUE(stepper.ready());

    mock_teardown();
}

    /*
     *  Check the state of the pins
     *
     *  They should follow a strict pattern
     *
     *  1000
     *  1001
     *  0011
     *  0010
     *  0110
     *  0100
     *  1100
     *
     *  repeated
     */

TEST(Motor, IO)
{
    mock_setup();

    int cycle = 5000;
    Stepper stepper(cycle, 1, 2, 3, 4);

    int pins[4] = { 0, 0, 0, 0 };

    EXPECT_EQ(stepper.position(), 0);

    // starts with begining pin hi
    pins[0] = 1;
    EXPECT_TRUE(pins_match(4, 1, pins));
 
    // one pin changes state on each step
    seek_test(& stepper, 1);
    pins[3] = 1;
    EXPECT_TRUE(pins_match(4, 1, pins));

    seek_test(& stepper, 2);
    pins[0] = 0;
    EXPECT_TRUE(pins_match(4, 1, pins));

    seek_test(& stepper, 3);
    pins[2] = 1;
    EXPECT_TRUE(pins_match(4, 1, pins));

    seek_test(& stepper, 4);
    pins[3] = 0;
    EXPECT_TRUE(pins_match(4, 1, pins));

    seek_test(& stepper, 5);
    pins[1] = 1;
    EXPECT_TRUE(pins_match(4, 1, pins));

    seek_test(& stepper, 6);
    pins[2] = 0;
    EXPECT_TRUE(pins_match(4, 1, pins));

    seek_test(& stepper, 7);
    pins[0] = 1;
    EXPECT_TRUE(pins_match(4, 1, pins));

    // back to the start of the pin sequence
    seek_test(& stepper, 8);
    pins[1] = 0;
    EXPECT_TRUE(pins_match(4, 1, pins));

    mock_teardown();
}

    /*
     *
     */

TEST(Motor, Clip)
{
    mock_setup();
    int cycle = 5000;
    Stepper stepper(cycle, 1, 2, 3, 4);

    EXPECT_EQ(stepper.position(), 0);

    // should seek to cycle-1
    move_to(& stepper, cycle + 1000);
    EXPECT_EQ(stepper.position(), cycle - 1);

    // should seek to 0
    move_to(& stepper, -1000);
    EXPECT_EQ(stepper.position(), 0);

    mock_teardown();
}

    /*
     *
     */

TEST(Motor, Rotate)
{
    mock_setup();
    int cycle = 5000;
    Stepper stepper(cycle, 1, 2, 3, 4);

    EXPECT_EQ(stepper.position(), 0);

    stepper.seek(200);
    move(& stepper, 200);
    EXPECT_EQ(stepper.position(), 200);

    // make it go backwards, through 0
    stepper.rotate(-100);
    stepper.poll();
    EXPECT_EQ(stepper.position(), 199);
    stepper.poll();
    EXPECT_EQ(stepper.position(), 198);
    // ...

    // move to 0, on its way to the target
    move(& stepper, 0);
    EXPECT_FALSE(stepper.ready());

    // Next poll should move to cycle-1
    stepper.poll();
    EXPECT_EQ(stepper.position(), cycle-1);
    EXPECT_FALSE(stepper.ready());

    move(& stepper, cycle - 100);
    // should now have stopped moving
    EXPECT_TRUE(stepper.ready());

    mock_teardown();
}

    /*
     *
     */

static int mod(Stepper *stepper, int t)
{
    const int cycle = stepper->get_steps();

    while (t < 0)
    {
        t += cycle;
    }
    while (t >= cycle)
    {
        t -= cycle;
    }
    return t;
}

static void quadrant(Stepper *stepper, int start, bool q1, bool q2, bool q3)
{
    start = stepper->clip(start);

    // reset to start
    stepper->seek(start);
    move(stepper, start);
    EXPECT_EQ(stepper->position(), start);

    // forwards
    int to = mod(stepper, start + 179);
    stepper->rotate(to);
    stepper->poll();
    EXPECT_EQ(stepper->position(), mod(stepper, start+1));
    stepper->poll();
    EXPECT_EQ(stepper->position(), mod(stepper, start+2));
    // ...

    bool zero;
    move(stepper, to, & zero);
    EXPECT_TRUE(stepper->ready());
    // do not pass 0
    EXPECT_EQ(zero, q1);    

    // reset to start
    stepper->seek(start);
    move(stepper, start);
    EXPECT_EQ(stepper->position(), start);

    // backwards
    to = mod(stepper, start + 181);
    stepper->rotate(to);
    stepper->poll();
    EXPECT_EQ(stepper->position(), mod(stepper, start-1));
    stepper->poll();
    EXPECT_EQ(stepper->position(), mod(stepper, start-2));
    // ...

    move(stepper, to, & zero);
    EXPECT_TRUE(stepper->ready());
    // pass 0
    EXPECT_EQ(zero, q2);

    // reset to start
    stepper->seek(start);
    move(stepper, start);
    EXPECT_EQ(stepper->position(), start);

    // backwards
    to = mod(stepper, start + 270);
    stepper->rotate(to);
    stepper->poll();
    EXPECT_EQ(stepper->position(), mod(stepper, start-1));
    stepper->poll();
    EXPECT_EQ(stepper->position(), mod(stepper, start-2));
    // ...

    move(stepper, to, & zero);
    EXPECT_TRUE(stepper->ready());
    // pass 0
    EXPECT_EQ(zero, q3);
}

TEST(Motor, RotateQuadrants)
{
    mock_setup();
    int cycle = 360;
    Stepper stepper(cycle, 1, 2, 3, 4);

    EXPECT_EQ(stepper.position(), 0);

    quadrant(& stepper, 45, false, true, true);
    quadrant(& stepper, 45+90, false, true, false);
    quadrant(& stepper, 45+180, true, false, false);

    mock_teardown();
}

//  FIN
