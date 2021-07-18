#include "mesh.h"
#include "renderer.h"

#include <cassert>
#include <tuple>
#include <queue>

Triangle::Triangle(const Vec3f& a_a, const Vec3f& a_b, const Vec3f& a_c)
	: a(a_a), b(a_b), c(a_c)
{
	n_a = n_b = n_c = (a_b - a_a).crossProduct(a_c - a_a);
}

Triangle::Triangle(const Vec3f& a_a, const Vec3f& a_b, const Vec3f& a_c, const Vec3f& a_n_a,
	const Vec3f& a_n_b, const Vec3f& a_n_c)
	: Triangle(a_a, a_b, a_c)
{
	n_a = a_n_a;
	n_b = a_n_b;
	n_c = a_n_c;
}

bool Triangle::rayTriangleIntersect(const Vec3f& orig, const Vec3f& dir, const Triangle* triPtr,
	float& t, Vec2f& uv)
{
#ifdef _STATS
	rayTriTests.store(rayTriTests.load() + 1);
#endif // _STATS
	const Vec3f& v0 = triPtr->a;
	const Vec3f& v1 = triPtr->b;
	const Vec3f& v2 = triPtr->c;
#if 0
	// compute plane's normal
	Vec3f v0v1 = ;
	Vec3f v0v2 = ;
	// no need to normalize
	Vec3f N = (v1 - v0).crossProduct(v2 - v0); // N
	//Vec3f normal = (tri.n_a + tri.n_b + tri.n_c).normalize();
	// std::cout << N << " " << normal << '\n';


	float area2 = N.length();


	// Step 1: finding P

	// check if ray and plane are parallel ?
	float NdotRayDirection = N.dotProduct(dir);
	if (fabs(NdotRayDirection) < 1e-8) // almost 0 
		return false; // they are parallel so they don't intersect ! 

	// compute d parameter using equation 2
	float d = N.dotProduct(v0);

	// compute t (equation 3)
	t = (N.dotProduct(orig) + d) / NdotRayDirection;
	// check if the triangle is in behind the ray
	if (t < 0) return false; // the triangle is behind 

	// compute the intersection point using equation 1
	Vec3f P = orig + t * dir;

	// Step 2: inside-outside test
	Vec3f C; // vector perpendicular to triangle's plane 

	// edge 0
	Vec3f edge0 = v1 - v0;
	Vec3f vp0 = P - v0;
	C = edge0.crossProduct(vp0);
	if (N.dotProduct(C) < 0) return false; // P is on the right side 

	// edge 1
	Vec3f edge1 = v2 - v1;
	Vec3f vp1 = P - v1;
	C = edge1.crossProduct(vp1);
	if (N.dotProduct(C) < 0)  return false; // P is on the right side 

	// edge 2
	Vec3f edge2 = v0 - v2;
	Vec3f vp2 = P - v2;
	C = edge2.crossProduct(vp2);
	if (N.dotProduct(C) < 0) return false; // P is on the right side; 

	return true; // this ray hits the triangle 
#else
	float u, v;
	Vec3f v0v1 = v1 - v0;
	Vec3f v0v2 = v2 - v0;
	Vec3f pvec = dir.crossProduct(v0v2);
	float det = v0v1.dotProduct(pvec);

	// backface culling
	// if (det < 1e-8) return false;

	// ray and triangle are parallel if det is close to 0
	if (fabs(det) < 1e-8) return false;

	float invDet = 1 / det;

	Vec3f tvec = orig - v0;

	u = tvec.dotProduct(pvec) * invDet;
	if (u < 0 || u > 1) return false;

	Vec3f qvec = tvec.crossProduct(v0v1);
	v = dir.dotProduct(qvec) * invDet;
	if (v < 0 || u + v > 1) return false;

	t = v0v2.dotProduct(qvec) * invDet;
	if (t < 0) return false;

	uv.x = u; uv.y = v;
	return true;
#endif
}


Mesh::Mesh()
{
	objectType = ObjectType::Mesh;
}

Mesh::~Mesh()
{
	for (const Triangle* tri : allTris)
		delete tri;
}

bool Mesh::intersectObject(const Vec3f& orig, const Vec3f& dir, float& t0, Vec2f& uv) const
{
	std::cout << "Object intersect called with mesh\n";
	std::exit(-1);
}

bool Mesh::intersectMesh(const Vec3f& orig, const Vec3f& dir, float& t0, const Triangle*& triPtr, 
	Vec2f& uv) const
{
	return ac->intersectAccelStruct(orig, dir, t0, triPtr, uv);
}

void Mesh::getSurfaceData(const Vec3f& hitPoint, const Triangle* const triPtr, const Vec2f& uv, 
	Vec3f& hitNormal, Vec2f& tex) const
{
#if 1
	// vertex normal shading
	hitNormal = ((triPtr->n_b * uv.x + triPtr->n_c * uv.y + triPtr->n_a * (1 - uv.x - uv.y)) / 3).normalize();
#else
	// triangle normal shading
	const Vec3f& v0 = triPtr->a;
	const Vec3f& v1 = triPtr->b;
	const Vec3f& v2 = triPtr->c;
	hitNormal = (v1 - v0).crossProduct(v2 - v0).normalize();
#endif
	tex = Vec2f{ uv.x, uv.y };
}


AccelerationStructure::AccelerationStructure(int a_depth)
	: depth(a_depth) {}

AccelerationStructure::~AccelerationStructure(){}

void AccelerationStructure::setTris(std::vector<const Triangle*>& a_tris)
{
#ifdef _NO_ACCEL_STRUCT
	tris = a_tris;
#endif // _NO_ACCEL_STRUCT

	using Pair = std::pair<std::vector<const Triangle*>*, AccelerationStructure*>;
	std::queue<Pair> queue;
	queue.push(Pair(&a_tris, this));

	std::vector<const Triangle*>* vec;
	AccelerationStructure* ac;

	while (!queue.empty()) {
		vec = queue.front().first;
		ac = queue.front().second;
		queue.pop();
		if (vec->size() < 10 || ac->depth > 10) {
			ac->tris = *vec;
			continue;
		}

		int split; // xyz split
		Vec3f dim = bounds[1] - bounds[0];
		if (dim.x > dim.y && dim.x > dim.z)
			split = 0;
		else if (dim.y > dim.z)
			split = 1;
		else
			split = 2;
		float avg = 0.0f;
		std::vector<const Triangle*> trisLeft;
		std::vector<const Triangle*> trisRight;

		if (split == 0) {
			for (const Triangle* const tri : a_tris)
				avg += tri->a.x + tri->b.x + tri->c.x;
			avg /= 3.0f * a_tris.size();
			for (const Triangle* const tri : a_tris) {
				if (tri->a.x <= avg || tri->b.x <= avg || tri->c.x <= avg)
					trisLeft.push_back(tri);
				if (tri->a.x >= avg || tri->b.x >= avg || tri->c.x >= avg)
					trisRight.push_back(tri);
			}
		}
		else if (split == 1) {
			for (const Triangle* tri : a_tris)
				avg += tri->a.y + tri->b.y + tri->c.y;
			avg /= 3.0f * a_tris.size();
			for (const Triangle* tri : a_tris) {
				if (tri->a.y <= avg || tri->b.y <= avg || tri->c.y <= avg)
					trisLeft.push_back(tri);
				if (tri->a.y >= avg || tri->b.y >= avg || tri->c.y >= avg)
					trisRight.push_back(tri);
			}
		}
		else if (split == 2) {
			for (const Triangle* tri : a_tris)
				avg += tri->a.z + tri->b.z + tri->c.z;
			avg /= 3.0f * a_tris.size();
			for (const Triangle* tri : a_tris) {
				if (tri->a.z <= avg || tri->b.z <= avg || tri->c.z <= avg)
					trisLeft.push_back(tri);
				if (tri->a.z >= avg || tri->b.z >= avg || tri->c.z >= avg)
					trisRight.push_back(tri);
			}
		}
		else {
			assert(false);
		}

		if (split == 0) {
			left = std::make_unique<AccelerationStructure>(ac->depth + 1);
			left->setBounds(bounds[0], Vec3f{ avg, bounds[1].y, bounds[1].z });
			left->setTris(trisLeft);
			right = std::make_unique<AccelerationStructure>(ac->depth + 1);
			right->setBounds(Vec3f{ avg, bounds[0].y, bounds[0].z }, bounds[1]);
			right->setTris(trisRight);
		}
		else if (split == 1) {
			left = std::make_unique<AccelerationStructure>(ac->depth + 1);
			left->setBounds(bounds[0], Vec3f{ bounds[1].x, avg, bounds[1].z });
			left->setTris(trisLeft);
			right = std::make_unique<AccelerationStructure>(ac->depth + 1);
			right->setBounds(Vec3f{ bounds[0].x, avg, bounds[0].z }, bounds[1]);
			right->setTris(trisRight);
		}
		else {
			left = std::make_unique<AccelerationStructure>(ac->depth + 1);
			left->setBounds(bounds[0], Vec3f{ bounds[1].x, bounds[1].y, avg });
			left->setTris(trisLeft);
			right = std::make_unique<AccelerationStructure>(ac->depth + 1);
			right->setBounds(Vec3f{ bounds[0].x, bounds[0].y, avg }, bounds[1]);
			right->setTris(trisRight);
		}
	}
}

void AccelerationStructure::setBounds(const Vec3f& a, const Vec3f& b)
{
	bounds[0] = a;
	bounds[1] = b;
}

bool AccelerationStructure::intersectBox(const Vec3f& orig, const Vec3f& dir) const
{
#ifdef _NO_ACCEL_STRUCT
	return true;
#endif // _NO_ACCEL_STRUCT
#ifdef _STATS
	accelStructTests.store(accelStructTests.load() + 1);
#endif // _STATS
	const Vec3f invdir = 1 / dir;
	const int sign[3] = { (invdir.x < 0), (invdir.y < 0), (invdir.z < 0) };

	float tmin, tmax, tymin, tymax, tzmin, tzmax;
	tmin = (bounds[sign[0]].x - orig.x) * invdir.x;
	tmax = (bounds[1 - sign[0]].x - orig.x) * invdir.x;
	tymin = (bounds[sign[1]].y - orig.y) * invdir.y;
	tymax = (bounds[1 - sign[1]].y - orig.y) * invdir.y;

	if ((tmin > tymax) || (tymin > tmax))
		return false;
	if (tymin > tmin)
		tmin = tymin;
	if (tymax < tmax)
		tmax = tymax;

	tzmin = (bounds[sign[2]].z - orig.z) * invdir.z;
	tzmax = (bounds[1 - sign[2]].z - orig.z) * invdir.z;

	if ((tmin > tzmax) || (tzmin > tmax))
		return false;
	if (tzmin > tmin)
		tmin = tzmin;
	if (tzmax < tmax)
		tmax = tzmax;

	return true;
}

bool AccelerationStructure::intersectAccelStruct(const Vec3f& orig, const Vec3f& dir, float& t0, 
	const Triangle*& triPtr, Vec2f& uv) const
{
	if (!this->intersectBox(orig, dir))
		return false;

	bool inter = false;
	float tempT;
	Vec2f tempUV;
	const Triangle* tempTriPtr;
	t0 = std::numeric_limits<float>::max();
	if (left != nullptr) {
		assert(right != nullptr);
		if (left->intersectAccelStruct(orig, dir, tempT, tempTriPtr, tempUV) && tempT < t0) {
			inter = true;
			t0 = tempT;
			uv = tempUV;
			triPtr = tempTriPtr;
		}
		if (right->intersectAccelStruct(orig, dir, tempT, tempTriPtr, tempUV) && tempT < t0) {
			inter = true;
			t0 = tempT;
			uv = tempUV;
			triPtr = tempTriPtr;
		}


		return inter;
	}

	for (const Triangle* tri : tris) {
		if (Triangle::rayTriangleIntersect(orig, dir, tri, tempT, tempUV) && tempT < t0) {
			inter = true;
			t0 = tempT;
			uv = tempUV;
			triPtr = tri;
		}
	}
	return inter;
}
