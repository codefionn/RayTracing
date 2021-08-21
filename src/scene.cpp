// classed describing scene and camera
#include "scene.h"

#include <thread>
#include <map>
#include <fstream>

#include "timer.h"
#include "util.h"
#include "options.h"
#include "stats.h"

Camera::Camera(const Vec3f& a_pos, const Vec3f& a_rot)
	: pos(a_pos), rot(a_rot) {}

Ray Camera::getRay(const float xPix, const  float yPix)
{
	if (!cameraRotated) {
		cameraRotated = true;
		const float& x = degToRad(rot.x);
		Matrix44f mx(
			1, 0, 0, 0,
			0, cosf(x), -sinf(x), 0,
			0, sinf(x), cosf(x), 0,
			0, 0, 0, 1
		);

		const float& y = degToRad(rot.y);
		Matrix44f my(
			cosf(y), 0, sinf(y), 0,
			0, 1, 0, 0,
			-sinf(y), 0, cosf(y), 0,
			0, 0, 0, 1
		);

		const float& z = degToRad(rot.z);
		Matrix44f mz(
			cosf(z), -sinf(z), 0, 0,
			sinf(z), cosf(z), 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1
		);

		rMatrix = mz * my * mx;
	}

	Vec3f dir = rMatrix.multVecMatrix(Vec3f(xPix, yPix, -1).normalize());
	return Ray{ this->pos , dir };
}


Scene::Scene(const std::string& sceneName)
{
	loadSkybox();
	sceneLoadSuccess = this->loadScene(sceneName);
}

bool Scene::loadScene(const std::string& sceneName)
{
	if (options::enableOutput) {
		std::cout << "Loading scene " << sceneName << '\n';
	}

    enum class BlockType { None, Options, Light, Object };
    std::map<std::string, BlockType> blockMap;
    blockMap["[options]"] = BlockType::Options;
    blockMap["[light]"] = BlockType::Light;
    blockMap["[object]"] = BlockType::Object;
    blockMap["[end]"] = BlockType::None;
    BlockType blockType = BlockType::None;

    std::string scenePath = options.imagePath + "\\" + sceneName;
    std::string str;
    std::ifstream ifs(scenePath, std::ifstream::in);

    Light* light = nullptr;
    Object* object = nullptr;

    while (ifs.good()) {
        std::getline(ifs, str);
        if (str.length() == 0)
            continue;

        // finish previous block
        if (strContains(str, "[")) {
            if (blockType == BlockType::Light) {
                assert(light != nullptr);
                lights.push_back(std::unique_ptr<Light>(light));
            }
            else if (blockType == BlockType::Object) {
                assert(object != nullptr);
                objects.push_back(std::unique_ptr<Object>(object));
            }
        }

        // skip commented block
        if (strContains(str, "#[")) {
            do {
                std::getline(ifs, str);
            } while (!strContains(str, "[") || strContains(str, "#["));
            if (!ifs.good())
                break;
        }

        // skip comments
        if (strContains(str, "#"))
            str.erase(str.find('#'));
        if (str.length() == 0)
            continue;

        // select block
        if (str[0] == '[') {
            assert(blockMap.find(str) != blockMap.end());
            blockType = blockMap[str];
            if (blockType == BlockType::None)
                break;
            continue;
        }

        // parse the line
        if (blockType == BlockType::Options) {
            assert(strContains(str, "="));
            std::string_view key(str.c_str(), str.find('='));
            std::string_view value(str.c_str() + str.find('=') + 1);

            if (strEquals(key, "width"))
                options.width = strToInt(value);
            else if (strEquals(key, "height"))
                options.height = strToInt(value);
            else if (strEquals(key, "fov"))
                options.fov = strToFloat(value);
            else if (strEquals(key, "image_name"))
                options.imageName = std::string(value);
            else if (strEquals(key, "n_workers="))
                options.nWorkers = strToInt(value);
            else if (strEquals(key, "max_ray_depth"))
                options.maxRayDepth = strToInt(value);
            else if (strEquals(key, "ac_penalty"))
                options.acPenalty = strToInt(value);
            else if (strEquals(key, "background_color"))
                options.backgroundColor = str3ToFloat(splitString(value, ','));
            else if (strEquals(key, "position"))
                camera.pos = str3ToFloat(splitString(value, ','));
            else if (strEquals(key, "rotation"))
                camera.rot = str3ToFloat(splitString(value, ','));
        }
        else if (blockType == BlockType::Light) {
            assert(strContains(str, "="));
            std::string_view key(str.c_str(), str.find('='));
            std::string_view value(str.c_str() + str.find('=') + 1);

            if (strEquals(key, "type")) {
                if (strEquals(value, "distant"))
                    light = new DistantLight();
                else if (strEquals(value, "point"))
                    light = new PointLight();
				else if (strEquals(value, "area"))
					light = new AreaLight();
            }
            else if (light == nullptr)
                std::cout << "Error, light type missing\n";
            else if (strEquals(key, "color"))
                light->color = str3ToFloat(splitString(value, ','));
            else if (strEquals(key, "intensity"))
                light->intensity = strToFloat(value);
            if (strEquals(key, "direction")) {
                assert(light->type == LightType::DistantLight);
                static_cast<DistantLight*>(light)->dir = str3ToFloat(splitString(value, ','));
            }
            else if (strEquals(key, "position")) {
                assert(light->type == LightType::PointLight);
                static_cast<PointLight*>(light)->pos = str3ToFloat(splitString(value, ','));
            }
			else if (strEquals(key, "pos")) {
				assert(light->type == LightType::AreaLight);
				static_cast<AreaLight*>(light)->pos = str3ToFloat(splitString(value, ','));
			}
			else if (strEquals(key, "i")) {
				assert(light->type == LightType::AreaLight);
				static_cast<AreaLight*>(light)->i = str3ToFloat(splitString(value, ','));
			}
			else if (strEquals(key, "j")) {
				assert(light->type == LightType::AreaLight);
				static_cast<AreaLight*>(light)->j = str3ToFloat(splitString(value, ','));
			}
			else if (strEquals(key, "samples")) {
				assert(light->type == LightType::AreaLight);
				static_cast<AreaLight*>(light)->samples = strToInt(value);
			}
			else if (strEquals(key, "base_samples")) {
				assert(light->type == LightType::AreaLight);
				static_cast<AreaLight*>(light)->base_samples = strToInt(value);
			}
        }
        else if (blockType == BlockType::Object) {
            assert(strContains(str, "="));
            std::string_view key(str.c_str(), str.find('='));
            std::string_view value(str.c_str() + str.find('=') + 1);

            if (strEquals(key, "type")) {
                if (strEquals(value, "plane"))
                    object = new Plane();
                else if (strEquals(value, "sphere"))
                    object = new Sphere();
                else if (strEquals(value, "mesh"))
                    object = new Mesh();
            }
            else if (object == nullptr) {
                std::cout << "Error, object type missing\n";
            }
            else if (strEquals(key, "color")) {
                object->color = str3ToFloat(splitString(value, ','));
            }
            else if (strEquals(key, "pos")) {
                object->pos = str3ToFloat(splitString(value, ','));
            }
            else if (strEquals(key, "pattern")) {
                if (strEquals(value, "chessboard")) {
                    object->pattern = PatternType::Chessboard;
                }
            }
            else if (strEquals(key, "material")) {
                auto res = splitString(value, ',');
                if (strEquals(res[0], "transparent")) {
                    object->materialType = MaterialType::Transparent;
					object->indexOfRefraction = strToFloat(res[1]);
                }
                else if (strEquals(res[0], "reflective")) {
                    object->materialType = MaterialType::Reflective;
                }
                if (strEquals(res[0], "phong")) {
                    object->materialType = MaterialType::Phong;
                    object->ambient = strToFloat(res[1]);
                    object->difuse = strToFloat(res[2]);
                    object->specular = strToFloat(res[3]);
                    object->nSpecular = strToFloat(res[4]);
                }
            }
            else if (object->objectType == ObjectType::Sphere) {
                Sphere* sphere = static_cast<Sphere*>(object);
                if (strEquals(key, "radius")) {
                    sphere->r = strToFloat(value);
                    sphere->r2 = powf(sphere->r, 2);
                }
            }
            else if (object->objectType == ObjectType::Plane) {
                Plane* plane = static_cast<Plane*>(object);
                if (strEquals(key, "normal")) {
                    plane->normal = str3ToFloat(splitString(value, ','));
                }
            }
            else if (object->objectType == ObjectType::Mesh) {
                Mesh* mesh = static_cast<Mesh*>(object);
                if (strEquals(key, "size")) {
                    mesh->size = str3ToFloat(splitString(value, ','));
                }
                else if (strEquals(key, "rot")) {
                    mesh->rot = str3ToFloat(splitString(value, ','));
                }
                else if (strEquals(key, "name")) {
                    mesh->loadOBJ(std::string(value), options);
                }
            }
        }
    }

    ifs.close();
    return true;
}

void Scene::loadSkybox()
{
	if (options::useSkybox) {
		const char names[][48] =
		{
			"D:\\dev\\CG\\RayTracing\\scenes\\box_left.bmp",
			"D:\\dev\\CG\\RayTracing\\scenes\\box_front.bmp",
			"D:\\dev\\CG\\RayTracing\\scenes\\box_right.bmp",
			"D:\\dev\\CG\\RayTracing\\scenes\\box_back.bmp",
			"D:\\dev\\CG\\RayTracing\\scenes\\box_top.bmp",
			"D:\\dev\\CG\\RayTracing\\scenes\\box_bottom.bmp"
		};

		unsigned char* u_skyboxes[6];
		int width, height;
		for (int i = 0; i < 6; i++) {
			u_skyboxes[i] = loadBMP(names[i], width, height);
		}
		skyboxHeight = height;
		skyboxWidth = width;

		for (int k = 0; k < 6; k++) {
			skyboxes[k] = new Vec3f[width * height];
			for (int i = 0; i < height; i++) {
				for (int j = 0; j < width; j++) {
					int v = 3 * (i * width + j);
					float x = u_skyboxes[k][v], y = u_skyboxes[k][v + 1], z = u_skyboxes[k][v + 2];
					x /= 256; y /= 256; z /= 256;
					skyboxes[k][i * width + j] = Vec3f{ x, y, z };
				}
			}
		}
	}
}

Vec3f Scene::getSkybox(const Vec3f& dir) const
{
	if (!options::useSkybox) {
		return options.backgroundColor;
	}

	auto toPixel = [](const float& v, const int& max)
	{
		int val = (v + 1.0f) / 2.0f * max;
		if (val >= max) val = max - 1;
		return val;
	};

	Vec3f adir = dir;
	float max = std::max(fabs(adir.x), std::max(fabs(adir.y), fabs(adir.z)));
	if (max == fabs(adir.z)) {
		if (adir.z < 0) {
			adir = dir * (1 / -dir.z);
			int i = (adir.y + 1.0f) / 2.0f * skyboxHeight;
			int j = (adir.x + 1.0f) / 2.0f * skyboxWidth;
			return skyboxes[1][i * skyboxWidth + j];
		}
		else {
			adir = dir * (1 / dir.z);
			int i = (adir.y + 1.0f) / 2.0f * skyboxHeight;
			int j = (-adir.x + 1.0f) / 2.0f * skyboxWidth;
			return skyboxes[3][i * skyboxWidth + j];
		}
	}
	else if (max == fabs(adir.x)) {
		if (adir.x < 0) {
			adir = dir * (1 / -dir.x);
			int i = toPixel(adir.y, skyboxHeight);
			int j = toPixel(-adir.z, skyboxWidth);
			return skyboxes[0][i * skyboxWidth + j];
		}
		else {
			adir = dir * (1 / dir.x);
			int i = toPixel(adir.y, skyboxHeight);
			int j = toPixel(adir.z, skyboxWidth);
			return skyboxes[2][i * skyboxWidth + j];
		}
	}
	else {
		if (adir.y < 0) {
			adir = dir * (1 / -dir.y);
			int i = toPixel(adir.z, skyboxHeight);
			int j = toPixel(adir.x, skyboxWidth);
			return skyboxes[5][i * skyboxWidth + j];
		}
		else {
			adir = dir * (1 / dir.y);
			int i = toPixel(adir.z, skyboxHeight);
			int j = toPixel(adir.x, skyboxWidth);
			return skyboxes[4][i * skyboxWidth + j];
		}
	}

	return options.backgroundColor;
}

void Scene::renderWorker(Vec3f* frameBuffer, size_t y0, size_t y1)
{
#ifdef _DEBUG
	if (options::outputProgress) {
		auto lastStat = std::chrono::high_resolution_clock::now();
	}
#endif // _DEBUG

	const float scale = tanf(options.fov * 0.5f / 180.0f * M_PI);
	const float imageAspectRatio = (options.width) / (float)options.height;
	for (size_t y = y0; y < y1; y++) {
		for (size_t x = 0; x < options.width; x++) {
			//if (x == 1921 && y == 1971) {
			//	frameBuffer[x + y * options.width] = { 1,0,0 };
			//	//continue;
			//}
			float xPix = (2 * (x + 0.5f) / (float)options.width - 1) * scale * imageAspectRatio;
			float yPix = -(2 * (y + 0.5f) / (float)options.height - 1) * scale;
			Ray ray = this->camera.getRay(xPix, yPix);
			frameBuffer[x + y * options.width] = Render::castRay(ray, *this, 0);
			finishedPixels.store(finishedPixels.load() + 1);
		}
#ifdef _DEBUG
		// in debug mode we only have one thread, so we need to check for progress updates manually
		if (options::outputProgress) {
			if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - lastStat).count() >= 1000ll) {
				const float progressCoef = 100.0f / (options.width * options.height);
				if (y % 10 == 0) std::cout << std::fixed << std::setw(2) << std::setprecision(0) << progressCoef * finishedPixels.load() << "%\n";
				lastStat = std::chrono::high_resolution_clock::now();
			}
		}
#endif // _DEBUG
	}
	finishedWorkers.store(finishedWorkers.load() + 1);
}

int Scene::launchWorkers(Vec3f* frameBuffer)
{
	Timer t("Render scene");

#ifdef _DEBUG
	// in debug mode we do not create any additional threads
	renderWorker(frameBuffer, 0, options.height);
#else
	std::vector<std::unique_ptr<std::thread>> threadPool;
	for (size_t i = 0; i < options.nWorkers; i++) {
		size_t y0 = options.height / options.nWorkers * i;
		size_t y1 = options.height / options.nWorkers * (i + 1);
		if (i + 1 == options.nWorkers) y1 = options.height;
		threadPool.push_back(std::make_unique<std::thread>(&Scene::renderWorker, this, frameBuffer, y0, y1));
	}

	if (options::outputProgress) {
		// until all workers finish we will print progress
		while (finishedWorkers.load() != options.nWorkers) {
			auto start = std::chrono::high_resolution_clock::now();
			for (int i = 0; i < 100; i++) {
				if (finishedWorkers.load() == options.nWorkers)
					break;
				if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count() >= 1000ll)
					break;
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count() >= 1000ll) {
				const float progressCoef = 100.0f / (options.width * options.height);
				std::cout << std::fixed << std::setw(2) << std::setprecision(0) << progressCoef * finishedPixels.load() << "%\n";
			}
		}
	}

	for (auto& thread : threadPool)
		thread->join();

#endif
	return 0;
}

long long Scene::render()
{
	if (!sceneLoadSuccess) return -1;
	Timer t("Total time");
	Vec3f* frameBuffer = new Vec3f[options.height * options.width];

	if (options::showAC) {
		const Vec3f orig = { 0, 0, 0 };
		const float scale = tanf(options.fov * 0.5f / 180.0f * M_PI);
		float imageAspectRatio = (options.width) / (float)options.height;

		int acMax = 0;
		int* acBuffer = new int[options.width * options.height];

		for (size_t y = 0; y < options.height; y++) {
			for (size_t x = 0; x < options.width; x++) {
				float xPix = (2 * (x + 0.5f) / (float)options.width - 1) * scale * imageAspectRatio;
				float yPix = -(2 * (y + 0.5f) / (float)options.height - 1) * scale;
				Ray ray = this->camera.getRay(xPix, yPix);
				int val = countAC(ray);
				if (val > acMax) acMax = val;
				acBuffer[x + y * options.width] = val;
			}
		}

		for (size_t y = 0; y < options.height; y++) {
			for (size_t x = 0; x < options.width; x++) {
				float val = acBuffer[x + y * options.width];
				val /= acMax;
				frameBuffer[x + y * options.width] = Vec3f{ val };
			}
		}
		delete[] acBuffer;
	}
	else {
		launchWorkers(frameBuffer);
	}

	if (options::imageOutput) {
		saveImage(frameBuffer, options);
	}

	delete[] frameBuffer;

	if (options::useSkybox) {
		for (int k = 0; k < 6; k++)
			if (skyboxes[k])
				delete[] skyboxes[k];
	}

	if (options::collectStatistics) {
		stats::printStats();
	}

	if (options::enableOutput) {
		std::cout << '\n';
	}
	return t.stop();
}

int Scene::countAC(const Ray& ray)
{
	int sum = 0;
	for (auto& obj : objects) {
		if (obj->objectType == ObjectType::Mesh) {
			Mesh* mesh = dynamic_cast<Mesh*>(obj.get());
			sum += mesh->ac.get()->recCountAC(ray);
		}
	}
	return sum;
}


Vec3f Render::reflect(const Vec3f& dir, const Vec3f& normal)
{
	return dir - 2 * dir.dotProduct(normal) * normal;
}

Vec3f Render::refract(const Vec3f& dir, const Vec3f& normal, const float& indexOfRefraction)
{
	float n1 = 1, n2 = indexOfRefraction;   // n1 - air, n2 - object
	float cosi = clamp(-1, 1, dir.dotProduct(normal));
	Vec3f modNormal = normal;
	if (cosi < 0) {
		// outside the medium
		cosi = -cosi;
	}
	else {
		// inside the medium
		std::swap(n1, n2);
		modNormal = -normal;
	}
	float rri = n1 / n2; // relative refraction index
	float k = 1 - rri * rri * (1 - cosi * cosi); // critial angle test
	if (k < 0)
		return 0;
	return rri * dir + (rri * cosi - sqrtf(k)) * modNormal;
}

float Render::fresnel(const Vec3f& dir, const Vec3f& normal, const float& indexOfRefraction)
{
	float n1 = 1, n2 = indexOfRefraction;   // n1 - air, n2 - object
	float cosi = clamp(-1, 1, dir.dotProduct(normal));
	if (cosi > 0) {
		// outside the medium
		std::swap(n1, n2);
	}
	float sint = n1 / n2 * sqrtf(std::max(0.f, 1 - cosi * cosi));  // Snell's law

	float kr;
	if (sint >= 1) {
		// total internal reflection
		kr = 1;
	}
	else {
		// fresnell equation
		float cost = sqrtf(std::max(0.f, 1 - sint * sint));
		cosi = fabsf(cosi);
		float rs = ((n2 * cosi) - (n1 * cost)) / ((n2 * cosi) + (n1 * cost));
		float rp = ((n1 * cosi) - (n2 * cost)) / ((n1 * cosi) + (n2 * cost));
		kr = (rs * rs + rp * rp) / 2;
	}
	return kr;
}

bool Render::trace(const Ray& ray, const ObjectVector& objects, IntersectInfo& intrInfo)
{
	intrInfo.hitObject = nullptr;
	for (auto i = objects.begin(); i != objects.end(); i++) {
		// transparent objects do not cast shadows
		if (ray.rayType == RayType::ShadowRay && (*i).get()->materialType == MaterialType::Transparent)
			continue;
		float tNear = std::numeric_limits<float>::max();
		const Triangle* ptr = nullptr;
		Vec2f uv;

		if ((*i).get()->objectType == ObjectType::Mesh) {
			if (dynamic_cast<Mesh*>((*i).get())->intersectMesh(ray, tNear, ptr, uv) && tNear < intrInfo.tNear) {
				intrInfo.hitObject = (*i).get();
				intrInfo.tNear = tNear;
				intrInfo.triPtr = ptr;
				intrInfo.uv = uv;
			}
		}
		else {
			if ((*i).get()->intersectObject(ray.orig, ray.dir, tNear, uv) && tNear < intrInfo.tNear) {
				intrInfo.hitObject = (*i).get();
				intrInfo.tNear = tNear;
				intrInfo.uv = uv;
			}
		}
	}
	return (intrInfo.hitObject != nullptr);
}

Vec3f Render::castRay(const Ray& ray, const Scene& scene, const int depth)
{
	if (depth > scene.options.maxRayDepth) return scene.getSkybox(ray.dir);
	IntersectInfo intrInfo;
	if (trace(ray, scene.objects, intrInfo)) {
		Vec3f hitColor = intrInfo.hitObject->color;
		Vec3f hitNormal;
		Vec2f hitTexCoordinates;
		Vec3f hitPoint = ray.orig + ray.dir * intrInfo.tNear;
		intrInfo.hitObject->getSurfaceData(hitPoint, intrInfo.triPtr, intrInfo.uv, hitNormal, hitTexCoordinates);

		Vec3f objectColor = intrInfo.hitObject->color;
		hitColor = { 0 };

		if (intrInfo.hitObject->materialType == MaterialType::Diffuse) {
			// for diffuse objects collect light from all visible sources
			for (size_t i = 0; i < scene.lights.size(); ++i) {
				if (scene.lights[i]->type == LightType::AreaLight) {
					AreaLight* light = dynamic_cast<AreaLight*>(scene.lights[i].get());
					Vec3f intensity = light->getTotalIlluminance(hitPoint + hitNormal * scene.options.bias, hitNormal, scene.objects);
					hitColor += objectColor * intensity;
				}
				else {
					IntersectInfo intrShadInfo;
					Vec3f lightDir, lightIntensity;
					scene.lights[i]->illuminate(hitPoint, lightDir, lightIntensity, intrShadInfo.tNear);
					bool vis = !trace(Ray{ hitPoint + hitNormal * scene.options.bias, -lightDir, RayType::ShadowRay }, scene.objects, intrShadInfo);
					float pattern = intrInfo.hitObject->getPattern(hitTexCoordinates);
					hitColor += (objectColor * lightIntensity) * pattern * vis * std::max(0.f, hitNormal.dotProduct(-lightDir));
				}
			}
		}
		else if (intrInfo.hitObject->materialType == MaterialType::Phong) {
			std::cout << "No phong yet\n";
			//Vec3f diffuseLight = 0, specularLight = 0;
			//for (uint32_t i = 0; i < scene.lights.size(); ++i) {
			//	Vec3f lightDir, lightIntensity;
			//	IntersectInfo isectShad;
			//	scene.lights[i]->illuminate(hitPoint, lightDir, lightIntensity, isectShad.tNear);

			//	bool vis = !trace(Ray{ hitPoint + hitNormal * scene.options.bias, -lightDir, RayType::ShadowRay }, scene.objects, isectShad);

			//	// compute the diffuse component
			//	diffuseLight += vis * intrInfo.hitObject->difuse * lightIntensity * std::max(0.f, hitNormal.dotProduct(-lightDir));

			//	// compute the specular component
			//	// what would be the ideal reflection direction for this light ray
			//	Vec3f R = reflect(lightDir, hitNormal);
			//	specularLight += vis * intrInfo.hitObject->specular * lightIntensity * std::pow(std::max(0.f, R.dotProduct(-ray.dir)), intrInfo.hitObject->nSpecular); // * intrInfo.hitObject->kReflect)
			//		// + (intrInfo.hitObject->color * (1 - intrInfo.hitObject->kReflect));
			//}
			//hitColor = objectColor * intrInfo.hitObject->ambient + diffuseLight + specularLight;
			//hitColor = hitColor * intrInfo.hitObject->getPattern(hitTexCoordinates);
		}
		else if (intrInfo.hitObject->materialType == MaterialType::Reflective) {
			// get info from reflected ray
			Ray reflectedRay{ hitPoint + scene.options.bias * hitNormal, ray.dir - 2 * ray.dir.dotProduct(hitNormal) * hitNormal };
			hitColor = castRay(reflectedRay, scene, depth + 1);
			hitColor *= 0.8f;
		}
		else if (intrInfo.hitObject->materialType == MaterialType::Transparent) {
			float kr = fresnel(ray.dir, hitNormal, intrInfo.hitObject->indexOfRefraction);
			bool outside = ray.dir.dotProduct(hitNormal) < 0;
			Vec3f biasVec = scene.options.bias * hitNormal;
			hitColor = { 0 };
			if (kr < 1) {
				// compute refraction if it is not a case of total internal reflection
				Vec3f refractionDirection = refract(ray.dir, hitNormal, intrInfo.hitObject->indexOfRefraction).normalize();
				Vec3f refractionRayOrig = outside ? hitPoint - biasVec : hitPoint + biasVec; // add bias
				Vec3f refractionColor = castRay(Ray{ refractionRayOrig, refractionDirection }, scene, depth + 1);
				hitColor += refractionColor * (1 - kr);
			}

			Vec3f reflectionDirection = reflect(ray.dir, hitNormal).normalize();
			Vec3f reflectionRayOrig = outside ? hitPoint + biasVec : hitPoint - biasVec;    // add bias
			Vec3f reflectionColor = castRay(Ray{ reflectionRayOrig, reflectionDirection }, scene, depth + 1);
			hitColor += reflectionColor * kr;
		}
		
		return hitColor;
	}
	return scene.getSkybox(ray.dir);
}

Vec3f AreaLight::getTotalIlluminance(const Vec3f& hitPoint, const Vec3f& hitNormal, const ObjectVector& objects)
{
	setPoints();
	float sumIntensity = 0;
	Vec3f lightIntensity = color * std::min(1.0f, (float)(intensity / (4 * M_PI * (hitPoint - pos).length2() / 1000)));

	if (options::useAreaLightAcceleration && base_samples > 0) {
		// try visibility of base points
		if (basePoints.size() < points.size()) {
			int visiblePoints = 0;
			int testedPoints = 0;
			for (const auto& p : basePoints) {
				IntersectInfo intrShadInfo;
				Vec3f dir = hitPoint - p;
				intrShadInfo.tNear = dir.length();
				visiblePoints += !Render::trace(Ray{ hitPoint, -dir.normalize(), RayType::ShadowRay }, objects, intrShadInfo);
				sumIntensity += std::max(0.f, hitNormal.dotProduct(-dir));
				testedPoints++;

				if (visiblePoints != 0 && visiblePoints != testedPoints)
					break;
			}

			// if no or all points visible, we may not compute all rest points
			if (visiblePoints == 0)
				return 0;
			// More agressive optimization, but may produce artifacts
			if (visiblePoints == basePoints.size())
				return lightIntensity * (sumIntensity / basePoints.size());
		}
	}

	// for maximum quality, test all samples
	for (const auto& p : points) {
		IntersectInfo intrShadInfo;
		Vec3f dir = hitPoint - p;
		intrShadInfo.tNear = dir.length();
		bool vis = !Render::trace(Ray{ hitPoint, -dir.normalize(), RayType::ShadowRay }, objects, intrShadInfo);
		sumIntensity += vis * std::max(0.f, hitNormal.dotProduct(-dir));
	}
	return lightIntensity * (sumIntensity / points.size());
}
