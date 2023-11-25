#include "math_util.h"
#include <svd3.h>

void SingularValueDecomposition(const glm::mat3& mat, glm::mat3* u, glm::mat3* s, glm::mat3* v)
{
	float* u11{ reinterpret_cast<float*>(u) + 0 };
	float* u12{ reinterpret_cast<float*>(u) + 3 };
	float* u13{ reinterpret_cast<float*>(u) + 6 };
	float* u21{ reinterpret_cast<float*>(u) + 1 };
	float* u22{ reinterpret_cast<float*>(u) + 4 };
	float* u23{ reinterpret_cast<float*>(u) + 7 };
	float* u31{ reinterpret_cast<float*>(u) + 2 };
	float* u32{ reinterpret_cast<float*>(u) + 5 };
	float* u33{ reinterpret_cast<float*>(u) + 8 };

	float* s11{ reinterpret_cast<float*>(s) + 0 };
	float* s12{ reinterpret_cast<float*>(s) + 3 };
	float* s13{ reinterpret_cast<float*>(s) + 6 };
	float* s21{ reinterpret_cast<float*>(s) + 1 };
	float* s22{ reinterpret_cast<float*>(s) + 4 };
	float* s23{ reinterpret_cast<float*>(s) + 7 };
	float* s31{ reinterpret_cast<float*>(s) + 2 };
	float* s32{ reinterpret_cast<float*>(s) + 5 };
	float* s33{ reinterpret_cast<float*>(s) + 8 };

	float* v11{ reinterpret_cast<float*>(v) + 0 };
	float* v12{ reinterpret_cast<float*>(v) + 3 };
	float* v13{ reinterpret_cast<float*>(v) + 6 };
	float* v21{ reinterpret_cast<float*>(v) + 1 };
	float* v22{ reinterpret_cast<float*>(v) + 4 };
	float* v23{ reinterpret_cast<float*>(v) + 7 };
	float* v31{ reinterpret_cast<float*>(v) + 2 };
	float* v32{ reinterpret_cast<float*>(v) + 5 };
	float* v33{ reinterpret_cast<float*>(v) + 8 };

	svd(mat[0][0], mat[1][0], mat[2][0], mat[0][1], mat[1][1], mat[2][1], mat[0][2], mat[1][2], mat[2][2],
		*u11, *u12, *u13, *u21, *u22, *u23, *u31, *u32, *u33,
		*s11, *s12, *s13, *s21, *s22, *s23, *s31, *s32, *s33,
		*v11, *v12, *v13, *v21, *v22, *v23, *v31, *v32, *v33);

	*s12 = 0.0f;
	*s13 = 0.0f;
	*s21 = 0.0f;
	*s23 = 0.0f;
	*s31 = 0.0f;
	*s32 = 0.0f;
}
