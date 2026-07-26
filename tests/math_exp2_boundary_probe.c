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

typedef double (*route_fn)(double);

typedef struct {
	const char *name;
	route_fn evaluate;
	unsigned int gate_bit;
	unsigned int special_failures;
	unsigned int sweep_failures;
	int first_failure_n;
	double first_actual;
	double first_expected;
	uint64_t max_ulp;
	int max_ulp_n;
} route_result;

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

static int v_alike(double a, double b)
{
	uint64_t a_bits = bits_of(a);
	uint64_t b_bits = bits_of(b);
	uint64_t a_magnitude = a_bits & 0x7fffffffffffffffULL;
	uint64_t b_magnitude = b_bits & 0x7fffffffffffffffULL;

	if ((a_bits & 0xfffffffffffffffcULL) == (b_bits & 0xfffffffffffffffcULL))
		return 1;
	if (a == 0.0 && b == 0.0)
		return 1;
	if (a_magnitude > 0x7ff0000000000000ULL &&
		b_magnitude > 0x7ff0000000000000ULL)
		return 1;
	if (a == b)
		return (a_bits >> 63) == (b_bits >> 63);
	return 0;
}

/* Exact C equivalent of math.veryclose(actual, expected). */
static int v_veryclose(double actual, double expected)
{
	double difference;
	double error_tolerance = 4e-16;

	if (actual == expected)
		return 1;
	difference = actual - expected;
	if (difference < 0.0)
		difference = -difference;
	if (expected != 0.0) {
		error_tolerance *= expected;
		if (error_tolerance < 0.0)
			error_tolerance = -error_tolerance;
	}
	return difference < error_tolerance;
}

static uint64_t ulp_distance(double actual, double expected)
{
	uint64_t actual_bits = bits_of(actual);
	uint64_t expected_bits = bits_of(expected);

	return actual_bits >= expected_bits ?
		actual_bits - expected_bits : expected_bits - actual_bits;
}

static double exact_power_of_two(int exponent)
{
	if (exponent < -1022)
		return value_from_bits(1ULL << (exponent + 1074));
	return value_from_bits((uint64_t) (exponent + 1023) << 52);
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

static double route_inline_exp2(double x)
{
	return exp2(x);
}

static double route_pow(double x)
{
	return pow(2.0, x);
}

static double route_ldexp(double x)
{
	return exp2_via_scale(x, 0);
}

static double route_scalbn(double x)
{
	return exp2_via_scale(x, 1);
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
	route_result routes[] = {
		{"exp2 inline", route_inline_exp2, 0, 0, 0, 0, 0.0, 0.0, 0, -1074},
		{"pow(2,x)", route_pow, 1, 0, 0, 0, 0.0, 0.0, 0, -1074},
		{"ldexp split", route_ldexp, 2, 0, 0, 0, 0.0, 0.0, 0, -1074},
		{"scalbn split", route_scalbn, 4, 0, 0, 0, 0.0, 0.0, 0, -1074},
	};
	unsigned int i;
	unsigned int route_index;
	unsigned int gate_mask = 0;
	int exponent;

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
		for (route_index = 0;
			route_index < sizeof(routes) / sizeof(routes[0]);
			route_index++) {
			double actual = routes[route_index].evaluate(x);

			if (!v_alike(cases[i].v_expected, actual)) {
				routes[route_index].special_failures++;
				printf("  special_mismatch route=%s expected_bits=%016llx actual_bits=%016llx\n",
					routes[route_index].name,
					(unsigned long long) bits_of(cases[i].v_expected),
					(unsigned long long) bits_of(actual));
			}
		}
	}

	puts("integer sweep n=-1074..1023 with V math.veryclose semantics:");
	for (exponent = -1074; exponent < 1024; exponent++) {
		double expected = exact_power_of_two(exponent);

		for (route_index = 0;
			route_index < sizeof(routes) / sizeof(routes[0]);
			route_index++) {
			double actual = routes[route_index].evaluate((double) exponent);
			uint64_t distance = ulp_distance(actual, expected);

			if (exponent == -1022) {
				printf("  checkpoint n=-1022 route=%-12s expected_bits=%016llx"
					" actual_bits=%016llx ulp=%llu veryclose=%s\n",
					routes[route_index].name,
					(unsigned long long) bits_of(expected),
					(unsigned long long) bits_of(actual),
					(unsigned long long) distance,
					v_veryclose(actual, expected) ? "yes" : "no");
			}
			if (distance > routes[route_index].max_ulp) {
				routes[route_index].max_ulp = distance;
				routes[route_index].max_ulp_n = exponent;
			}
			if (!v_veryclose(actual, expected)) {
				if (routes[route_index].sweep_failures == 0) {
					routes[route_index].first_failure_n = exponent;
					routes[route_index].first_actual = actual;
					routes[route_index].first_expected = expected;
				}
				routes[route_index].sweep_failures++;
			}
		}
	}

	for (route_index = 0;
		route_index < sizeof(routes) / sizeof(routes[0]);
		route_index++) {
		route_result *route = &routes[route_index];

		printf("summary route=%-12s role=%s special_failures=%u sweep_failures=%u"
			" max_ulp=%llu max_ulp_n=%d\n",
			route->name,
			route->gate_bit == 0 ? "diagnostic" : "candidate",
			route->special_failures,
			route->sweep_failures,
			(unsigned long long) route->max_ulp,
			route->max_ulp_n);
		if (route->sweep_failures != 0) {
			printf("  first_sweep_failure n=%d expected_bits=%016llx actual_bits=%016llx"
				" ulp=%llu\n",
				route->first_failure_n,
				(unsigned long long) bits_of(route->first_expected),
				(unsigned long long) bits_of(route->first_actual),
				(unsigned long long) ulp_distance(route->first_actual,
					route->first_expected));
		}
		if (route->gate_bit != 0 &&
			(route->special_failures != 0 || route->sweep_failures != 0))
			gate_mask |= route->gate_bit;
	}
	printf("candidate_gate_mask=%u status=%s\n",
		gate_mask, gate_mask == 0 ? "pass" : "fail");
	return (int) gate_mask;
}
