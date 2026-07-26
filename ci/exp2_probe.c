#include <math.h>
#include <stdint.h>
#include <stdio.h>

typedef union {
    double value;
    uint64_t bits;
} double_bits;

static int failures;

static uint64_t to_bits(double value)
{
    double_bits converted;
    converted.value = value;
    return converted.bits;
}

static double from_bits(uint64_t bits)
{
    double_bits converted;
    converted.bits = bits;
    return converted.value;
}

static double call_exp2(double input)
{
    volatile double runtime_input = input;
    return exp2(runtime_input);
}

static void print_bits(uint64_t bits)
{
    printf("0x%08lx%08lx",
           (unsigned long)(bits >> 32),
           (unsigned long)(bits & 0xffffffffull));
}

static void expect_exact(const char *name, double input, uint64_t expected)
{
    uint64_t actual = to_bits(call_exp2(input));
    int pass = actual == expected;

    printf("%-18s input=% .17g actual=", name, input);
    print_bits(actual);
    printf(" expected=");
    print_bits(expected);
    printf(" %s\n", pass ? "PASS" : "FAIL");
    failures += !pass;
}

static void expect_near(const char *name, double input, uint64_t expected,
                        uint64_t max_ulps)
{
    uint64_t actual = to_bits(call_exp2(input));
    uint64_t distance = actual > expected ? actual - expected : expected - actual;
    int pass = distance <= max_ulps;

    printf("%-18s input=% .17g actual=", name, input);
    print_bits(actual);
    printf(" expected=");
    print_bits(expected);
    printf(" ulps=%lu %s\n", (unsigned long)distance,
           pass ? "PASS" : "FAIL");
    failures += !pass;
}

static void expect_nan(const char *name, double input)
{
    uint64_t actual = to_bits(call_exp2(input));
    int pass = (actual & 0x7ff0000000000000ull) == 0x7ff0000000000000ull
        && (actual & 0x000fffffffffffffull) != 0;

    printf("%-18s input=nan actual=", name);
    print_bits(actual);
    printf(" %s\n", pass ? "PASS" : "FAIL");
    failures += !pass;
}

int main(void)
{
    printf("exp2 probe: pointer_bits=%lu\n",
           (unsigned long)(8 * sizeof(void *)));

    expect_exact("normal -1", -1.0, 0x3fe0000000000000ull);
    expect_exact("normal 0", 0.0, 0x3ff0000000000000ull);
    expect_exact("normal 1", 1.0, 0x4000000000000000ull);
    expect_exact("normal 10", 10.0, 0x4090000000000000ull);
    expect_near("normal 0.5", 0.5, 0x3ff6a09e667f3bcdull, 2);

    expect_exact("+infinity", from_bits(0x7ff0000000000000ull),
                 0x7ff0000000000000ull);
    expect_exact("-infinity", from_bits(0xfff0000000000000ull),
                 0x0000000000000000ull);
    expect_nan("quiet nan", from_bits(0x7ff8000000000042ull));

    expect_exact("min normal -1022", -1022.0, 0x0010000000000000ull);
    expect_exact("subnormal -1023", -1023.0, 0x0008000000000000ull);
    expect_exact("subnormal -1073", -1073.0, 0x0000000000000002ull);
    expect_exact("true min -1074", -1074.0, 0x0000000000000001ull);
    expect_exact("rounds min -1074.5", -1074.5, 0x0000000000000001ull);
    expect_exact("underflow -1075", -1075.0, 0x0000000000000000ull);

    printf("exp2 probe: %s (%d failure%s)\n",
           failures ? "FAIL" : "PASS", failures,
           failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
