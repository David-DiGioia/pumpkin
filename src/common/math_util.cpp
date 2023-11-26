#include "math_util.h"
#include "SVD.h"
#include "polar_decomposition_3x3.h"

void SingularValueDecomposition(const glm::mat3& mat, glm::mat3* u, glm::mat3* s, glm::mat3* v)
{
	// Create a matrix
	SVD::Mat3x3 m = SVD::Mat3x3(mat[0][0], mat[1][0], mat[2][0], mat[0][1], mat[1][1], mat[2][1], mat[0][2], mat[1][2], mat[2][2]);
	// Call the actual SVD decomposition which returns a struct containing the 3 calculated matrices U, S, V
	auto [U, S, V] = SVD::svd(m);

	//// Reconstruct B from the SVD
	//auto B = U * S * V.transpose();
	//// Simple error metric
	//auto error = (m - B).det();

	(*u)[0][0] = U.m_00;
	(*u)[1][0] = U.m_01;
	(*u)[2][0] = U.m_02;
	(*u)[0][1] = U.m_10;
	(*u)[1][1] = U.m_11;
	(*u)[2][1] = U.m_12;
	(*u)[0][2] = U.m_20;
	(*u)[1][2] = U.m_21;
	(*u)[2][2] = U.m_22;

	(*s)[0][0] = S.m_00;
	(*s)[1][0] = S.m_01;
	(*s)[2][0] = S.m_02;
	(*s)[0][1] = S.m_10;
	(*s)[1][1] = S.m_11;
	(*s)[2][1] = S.m_12;
	(*s)[0][2] = S.m_20;
	(*s)[1][2] = S.m_21;
	(*s)[2][2] = S.m_22;

	(*v)[0][0] = V.m_00;
	(*v)[1][0] = V.m_01;
	(*v)[2][0] = V.m_02;
	(*v)[0][1] = V.m_10;
	(*v)[1][1] = V.m_11;
	(*v)[2][1] = V.m_12;
	(*v)[0][2] = V.m_20;
	(*v)[1][2] = V.m_21;
	(*v)[2][2] = V.m_22;
}

void PolarDecomposition(const glm::mat3& mat, glm::mat3* r, glm::mat3* s)
{
	polar::polar_decomposition<float>(reinterpret_cast<float*>(r), reinterpret_cast<float*>(s), reinterpret_cast<const float*>(&mat));
}
