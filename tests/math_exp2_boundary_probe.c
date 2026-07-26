#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef union {
	double value;
	uint64_t bits;
} double_bits;

typedef struct {
	const char *name;
	double input;
	double v_expected;
} probe_case;

static const double ln2 = 0.693147180559945309;

static uint64_t bits_of(double value)
{
	double_bits converted;
	converted.value = value;
	return converted.bits;
}

static double value_from_bits(uint64_t bits)
{
	double_bits converted;
	converted.bits = bits;
	return converted.value;
}

static const char *class_of(double value)
{
	uint64_t magnitude = bits_of(value) & 0x7fffffffffffffffULL;

	if (magnitude == 0)
		return "zero";
	if (magnitude < 0x0010000000000000ULL)
		return "subnormal";
	if (magnitude < 0x7ff0000000000000ULL)
		return "normal";
	if (magnitude == 0x7ff0000000000000ULL)
		return "infinity";
	return "nan";
}

static void print_value(const char *route, double value)
{
	printf("  %-18s value=% .17g bits=%016llx class=%s\n",
		route,
		value,
		(unsigned long long) bits_of(value),
		class_of(value));
}

static double exp2_via_scale(double x, int use_scalbn)
{
	uint64_t bits = bits_of(x);
	uint64_t magnitude = bits & 0x7fffffffffffffffULL;
	int exponent;
	double fraction;
	double mantissa;

	if (magnitude > 0x7ff0000000000000ULL)
		return x;
	if (magnitude == 0x7ff0000000000000ULL)
		return bits >> 63 ? 0.0 : x;

	exponent = (int) x;
	if ((double) exponent > x)
		exponent--;
	fraction = x - (double) exponent;
	mantissa = exp(fraction * ln2);
	return use_scalbn ? scalbn(mantissa, exponent) : ldexp(mantissa, exponent);
}

int main(void)
{
	char *literal_end = NULL;
	volatile double decimal_true_min = 5e-324;
	volatile double hex_true_min = 0x1p-1074;
	double parsed_true_min = strtod("5e-324", &literal_end);
	probe_case cases[] = {
		{"underflow", -2000.0, 0.0},
		{"overflow", 2000.0, value_from_bits(0x7ff0000000000000ULL)},
		{"positive-infinity", value_from_bits(0x7ff0000000000000ULL),
			value_from_bits(0x7ff0000000000000ULL)},
		{"nan", value_from_bits(0x7ff8000000000000ULL),
			value_from_bits(0x7ff8000000000000ULL)},
		{"overflow-boundary", 1024.0, value_from_bits(0x7ff0000000000000ULL)},
		{"subnormal-boundary", -1.07399999999999e+03, 5e-324},
		{"near-zero", 3.725290298461915e-09, 1.0000000025821745},
	};
	unsigned int i;

	printf("target_pointer_bits=%u\n", (unsigned int) (sizeof(void *) * 8));
	puts("true-min construction routes:");
	print_value("decimal 5e-324", decimal_true_min);
	print_value("hex 0x1p-1074", hex_true_min);
	print_value("bits 1", value_from_bits(1));
	print_value("strtod 5e-324", parsed_true_min);
	printf("  strtod_consumed_all=%s\n", literal_end != NULL && *literal_end == '\0' ? "yes" : "no");

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		volatile double x = cases[i].input;

		printf("case=%s\n", cases[i].name);
		print_value("input", x);
		print_value("V expected", cases[i].v_expected);
		print_value("exp2(x)", exp2(x));
		print_value("exp(x*ln2)", exp(x * ln2));
		print_value("pow(2,x)", pow(2.0, x));
		print_value("ldexp split", exp2_via_scale(x, 0));
		print_value("scalbn split", exp2_via_scale(x, 1));
	}
	return 0;
}
