struct HitPayload
{
	vec3 radiance;
	vec3 reflected_ratio; // Ratio of light reflected to camera.
	uint depth;           // Needed for random seed.
	uint sample_number;   // Needed for random seed.
	uint done;
	vec3 ray_origin;
	vec3 ray_direction;
};

const float pi = 3.14159265359;

// A single iteration of Bob Jenkins' One-At-A-Time hashing algorithm.
uint Hash(uint x)
{
    x += (x << 10u);
    x ^= (x >> 6u);
    x += (x << 3u);
    x ^= (x >> 11u);
    x += (x << 15u);
    return x;
}

// Compound versions of the hashing algorithm.
uint Hash(uvec2 v) { return Hash(v.x ^ Hash(v.y)); }
uint Hash(uvec3 v) { return Hash(v.x ^ Hash(v.y) ^ Hash(v.z)); }
uint Hash(uvec4 v) { return Hash(v.x ^ Hash(v.y) ^ Hash(v.z) ^ Hash(v.w)); }

// Construct a float with half-open range [0, 1) using low 23 bits.
// All zeroes yields 0.0, all ones yields the next smallest representable value below 1.0.
float FloatConstruct(uint m)
{
	const uint ieee_mantissa = 0x007FFFFFu; // binary32 mantissa bitmask.
	const uint ieee_one = 0x3F800000u; // 1.0 in IEEE binary32.

	m &= ieee_mantissa;                     // Keep only mantissa bits (fractional part).
	m |= ieee_one;                          // Add fractional part to 1.0.

	float  f = uintBitsToFloat(m);          // Range [1, 2).
	return f - 1.0;                         // Range [0, 1).
}

// Halton sequence generates random seeming numbers in (0, 1) by representing the index in specified base (eg binary for base==2)
// and putting it to the right of the decimal point and reversing the order of the digits.
float HaltonSequence(uint index, uint base)
{
	float reciprocal_power = 1;
	float result = 0;

	while (index > 0)
	{
		reciprocal_power /= base;
		result += reciprocal_power * (index % base); // Digit in specified base.
		index /= base;                               // Dividing by base is essentially a shift right in specified base.
	}

	return result;
}

float Rand(uint seed, float lower_bound, float upper_bound)
{
	return FloatConstruct(seed) * (upper_bound - lower_bound) + lower_bound;
}

// Get a random point on the surface of a unit sphere from a uniform distribution.
vec3 RandomPointOnUnitSphere(uint seed)
{
	// First pick the z-coordinate uniformly.
	float z = Rand(seed, -1.0, 1.0);
	// Then the x and y coordinates will lie on a circle of this radius.
	float radius = sqrt(1.0 - z * z);
	// Then x any y lie on that circle at random angle theta.
	float theta = Rand(Hash(seed), 0.0, 2.0 * pi);
	float x = radius * cos(theta);
	float y = radius * sin(theta);

	return vec3(x, y, z);
}

// Get a random point on the surface of a unit hemisphere centered on dir.
vec3 RandomPointOnUnitHemiSphere(uint seed, vec3 dir)
{
	vec3 on_sphere = RandomPointOnUnitSphere(seed);
	return dot(on_sphere, dir) < 0.0 ? -on_sphere : on_sphere;
}
