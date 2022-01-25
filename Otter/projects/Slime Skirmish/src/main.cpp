#include <Logging.h>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <json.hpp>
#include <fstream>
#include <sstream>
#include <typeindex>
#include <optional>
#include <string>
#include <random>

// GLM math library
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <GLM/gtx/common.hpp> // for fmod (floating modulus)

// Graphics
#include "Graphics/IndexBuffer.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/Shader.h"
#include "Graphics/Texture2D.h"
#include "Graphics/TextureCube.h"
#include "Graphics/VertexTypes.h"
#include "Graphics/Font.h"
#include "Graphics/GuiBatcher.h"

// Utilities
#include "Utils/MeshBuilder.h"
#include "Utils/MeshFactory.h"
#include "Utils/ObjLoader.h"
#include "Utils/ImGuiHelper.h"
#include "Utils/ResourceManager/ResourceManager.h"
#include "Utils/FileHelpers.h"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/StringUtils.h"
#include "Utils/GlmDefines.h"

// Gameplay
#include "Gameplay/Material.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/Scene.h"

// Components
#include "Gameplay/Components/IComponent.h"
#include "Gameplay/Components/Camera.h"
#include "Gameplay/Components/RotatingBehaviour.h"
#include "Gameplay/Components/JumpBehaviour.h"
#include "Gameplay/Components/RenderComponent.h"
#include "Gameplay/Components/MaterialSwapBehaviour.h"
#include "Gameplay/Components/AbilityComponent.h"
#include "Gameplay/Components/MovementComponent.h"

// Physics
#include "Gameplay/Physics/RigidBody.h"
#include "Gameplay/Physics/Colliders/BoxCollider.h"
#include "Gameplay/Physics/Colliders/PlaneCollider.h"
#include "Gameplay/Physics/Colliders/SphereCollider.h"
#include "Gameplay/Physics/Colliders/ConvexMeshCollider.h"
#include "Gameplay/Physics/TriggerVolume.h"
#include "Graphics/DebugDraw.h"
#include "Gameplay/Components/TriggerVolumeEnterBehaviour.h"
#include "Gameplay/Components/SimpleCameraControl.h"
#include "Gameplay/Physics/Colliders/CylinderCollider.h"

// GUI
#include "Gameplay/Components/GUI/RectTransform.h"
#include "Gameplay/Components/GUI/GuiPanel.h"
#include "Gameplay/Components/GUI/GuiText.h"
#include "Gameplay/InputEngine.h"

// Animations
#include "Animations/Animation.h"
#include "Animations/CAnimator.h"
#include "Animations/CSkinnedMeshRenderer.h"
#include "Animations/SkinnedMesh.h"

//#define LOG_GL_NOTIFICATIONS 

/*
	Handles debug messages from OpenGL
	https://www.khronos.org/opengl/wiki/Debug_Output#Message_Components
	@param source    Which part of OpenGL dispatched the message
	@param type      The type of message (ex: error, performance issues, deprecated behavior)
	@param id        The ID of the error or message (to distinguish between different types of errors, like nullref or index out of range)
	@param severity  The severity of the message (from High to Notification)
	@param length    The length of the message
	@param message   The human readable message from OpenGL
	@param userParam The pointer we set with glDebugMessageCallback (should be the game pointer)
*/
void GlDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) 
{
	std::string sourceTxt;
	switch (source) 
	{
		case GL_DEBUG_SOURCE_API: sourceTxt = "DEBUG"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM: sourceTxt = "WINDOW"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceTxt = "SHADER"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY: sourceTxt = "THIRD PARTY"; break;
		case GL_DEBUG_SOURCE_APPLICATION: sourceTxt = "APP"; break;
		case GL_DEBUG_SOURCE_OTHER: default: sourceTxt = "OTHER"; break;
	}
	switch (severity) 
	{
		case GL_DEBUG_SEVERITY_LOW:          LOG_INFO("[{}] {}", sourceTxt, message); break;
		case GL_DEBUG_SEVERITY_MEDIUM:       LOG_WARN("[{}] {}", sourceTxt, message); break;
		case GL_DEBUG_SEVERITY_HIGH:         LOG_ERROR("[{}] {}", sourceTxt, message); break;
			#ifdef LOG_GL_NOTIFICATIONS
		case GL_DEBUG_SEVERITY_NOTIFICATION: LOG_INFO("[{}] {}", sourceTxt, message); break;
			#endif
		default: break;
	}
}  

GLFWwindow* window;
glm::ivec2 windowSize = glm::ivec2(800, 800);
std::string windowTitle = "Slime Skirmish";

int waveLevel = 1;
bool planeSwitch = false;
int spawnRange = 15;
float planeDifference = 50.0f;
float slimeDamage = 10.0f, enemyDamage = 10.0f;
float t = 0.0f;

using namespace Gameplay;
using namespace Gameplay::Physics;

Scene::Sptr scene = nullptr;

int monitorVec[4];
GLFWmonitor* monitor; 

void GlfwWindowResizedCallback(GLFWwindow* window, int width, int height) 
{
	glViewport(0, 0, width, height);
	windowSize = glm::ivec2(width, height);

	if (windowSize.x * windowSize.y > 0) 
	{
		scene->MainCamera->ResizeWindow(width, height);
	}
	GuiBatcher::SetWindowSize({ width, height });
}

/// <summary>
/// Handles intializing GLFW, should be called before initGLAD, but after Logger::Init()
/// Also handles creating the GLFW window
/// </summary>
/// <returns>True if GLFW was initialized, false if otherwise</returns>
bool initGLFW() 
{
	if (glfwInit() == GLFW_FALSE) 
	{
		LOG_ERROR("Failed to initialize GLFW");
		return false;
	}

	window = glfwCreateWindow(windowSize.x, windowSize.y, windowTitle.c_str(), nullptr, nullptr);
	glfwMakeContextCurrent(window);

	//monitor = glfwGetPrimaryMonitor();
	//glfwGetMonitorWorkarea(monitor, &monitorVec[0], &monitorVec[1], &monitorVec[2], &monitorVec[3]);
	//glfwSetWindowSizeLimits(window, 16, 9, monitorVec[2], monitorVec[3]);
	//glfwSetWindowMonitor(window, monitor, 0, 0, monitorVec[2], monitorVec[3], GLFW_DONT_CARE);

	glfwSetWindowSizeCallback(window, GlfwWindowResizedCallback);

	InputEngine::Init(window);

	GuiBatcher::SetWindowSize(windowSize);

	return true;
}

/// <summary>
/// Handles initializing GLAD and preparing our GLFW window for OpenGL calls
/// </summary>
/// <returns>True if GLAD is loaded, false if there was an error</returns>
bool initGLAD() {
	if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) 
	{
		LOG_ERROR("Failed to initialize Glad");
		return false;
	}
	return true;
}

template<typename T>
T LERP(const T& p0, const T& p1, float t) 
{ 
	return (1.0f - t) * p0 + t * p1; 
}

/// <summary>
/// Draws a widget for saving or loading our scene
/// </summary>
/// <param name="scene">Reference to scene pointer</param>
/// <param name="path">Reference to path string storage</param>
/// <returns>True if a new scene has been loaded</returns>
bool DrawSaveLoadImGui(Scene::Sptr& scene, std::string& path) 
{
	ImGui::InputText("Path", path.data(), path.capacity());

	if (ImGui::Button("Save")) 
	{
		scene->Save(path);

		std::string newFilename = std::filesystem::path(path).stem().string() + "-manifest.json";
		ResourceManager::SaveManifest(newFilename);
	}
	ImGui::SameLine();

	if (ImGui::Button("Load")) 
	{
		scene = nullptr;

		std::string newFilename = std::filesystem::path(path).stem().string() + "-manifest.json";
		ResourceManager::LoadManifest(newFilename);
		scene = Scene::Load(path);

		return true;
	}
	return false;
}

/// <summary>
/// Draws some ImGui controls for the given light
/// </summary>
/// <param name="title">The title for the light's header</param>
/// <param name="light">The light to modify</param>
/// <returns>True if the parameters have changed, false if otherwise</returns>
bool DrawLightImGui(const Scene::Sptr& scene, const char* title, int ix) 
{
	bool isEdited = false;
	bool result = false;
	Light& light = scene->Lights[ix];
	ImGui::PushID(&light);
	if (ImGui::CollapsingHeader(title)) 
	{
		isEdited |= ImGui::DragFloat3("Pos", &light.Position.x, 0.01f);
		isEdited |= ImGui::ColorEdit3("Col", &light.Color.r);
		isEdited |= ImGui::DragFloat("Range", &light.Range, 0.1f);

		result = ImGui::Button("Delete");
	}
	if (isEdited) 
	{
		scene->SetShaderLight(ix);
	}

	ImGui::PopID();
	return result;
}

// Update camera to give a top down view
float cameraHeight = 10.0f;
float cameraDistance = 5.0f;
void TopDownCamera(GameObject* camera, GameObject::Sptr player)
{
	glm::vec3 cameraPosition = (glm::vec3(0.0f, -cameraDistance, 0.0f)) + (glm::vec3(0.0f, 0.0f, cameraHeight));
	camera->SetPostion(player->GetPosition() + cameraPosition);
	camera->LookAt(player->GetPosition());
}

// Slime uses attack and absorb
float abilityCooldown = 1.0f;
float nextAbility = 0.0f;
void UseAbility(GameObject::Sptr player, GameObject::Sptr enemy, double time)
{
	if (time > nextAbility)
	{
		if (enemy->GetHealth() <= 0.0f)
		{
			enemy->SetRotation(glm::vec3(0.0f));

			if (enemy->Get<TriggerVolumeEnterBehaviour>()->GetTrigger())
			{
				player->Get<AbilityComponent>()->SetType(AbilityComponent::AbilityType::Absorb);
			}
			else
			{
				player->Get<AbilityComponent>()->SetType(AbilityComponent::AbilityType::None);
			}

			if (player->Get<AbilityComponent>()->GetType() == AbilityComponent::AbilityType::Absorb && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
			{
				// Do ability
				player->SetScale(player->GetScale() + glm::vec3(0.1));
				player->SetHealth(player->GetHealth() + 5.0f);
				scene->RemoveGameObject(enemy->SelfRef());
				nextAbility = glfwGetTime() + abilityCooldown;
			}
		}
	}

	if (time > nextAbility)
	{

		if (enemy->Get<TriggerVolumeEnterBehaviour>()->GetTrigger())
		{
			player->Get<AbilityComponent>()->SetType(AbilityComponent::AbilityType::Attack);

			if (player->Get<AbilityComponent>()->GetType() == AbilityComponent::AbilityType::Attack && glfwGetMouseButton(window, 0) == GLFW_PRESS)
			{
				// Do attack
				enemy->SetHealth(enemy->GetHealth() - slimeDamage);
				nextAbility = glfwGetTime() + abilityCooldown;
			}
		}
	}
}

// Enemies AI movement
glm::vec3 movementVector;
float desiredVelocity;
void EnemySteeringBehaviour(GameObject::Sptr player, GameObject::Sptr enemy, float dt)
{
	if (enemy->GetHealth() >= 1.0f)
	{
		const int safeDistance = 10;
		glm::vec3 desiredDirection = player->GetPosition() - enemy->GetPosition();

		if (glm::length(desiredDirection) < safeDistance / 2)
		{
			desiredVelocity = glm::distance(enemy->GetPosition(), player->GetPosition());
			enemy->LookAt(player->GetPosition());
			movementVector = glm::normalize(desiredDirection);
		}
		else if (glm::length(desiredDirection) < safeDistance)
		{
			desiredVelocity = 2;
			enemy->LookAt(player->GetPosition());
			movementVector = glm::normalize(desiredDirection);
		}

		movementVector *= desiredVelocity * dt;
		enemy->SetPostion(enemy->GetPosition() + movementVector);
	}
	else
	{
		enemy->SetPostion(enemy->GetPosition());
	}
}

// Enemies damaging slime
float nextAttack = 0.0f;
float enemyCooldown = 2.0f;
void TakeDamage(GameObject::Sptr player, GameObject::Sptr enemy, double time)
{
	if (time > nextAttack)
	{
		if ((player->Get<TriggerVolumeEnterBehaviour>()->GetTrigger()) && (enemy->Get<TriggerVolumeEnterBehaviour>()->GetTrigger()))
		{
			player->SetHealth(player->GetHealth() - enemyDamage);
			nextAttack = glfwGetTime() + enemyCooldown;
		}
	}
}

// Create walls around plane
void CreateWalls(int index, GameObject::Sptr plane, MeshResource::Sptr wallMesh, Material::Sptr boxMaterial)
{
	GameObject::Sptr topWallLeft = scene->CreateGameObject("Top Wall Left" + std::to_string(index));
	{
		topWallLeft->SetPostion(glm::vec3(plane->GetPosition().x - 15, plane->GetPosition().y + 25, 10.0f));

		RenderComponent::Sptr renderer = topWallLeft->Add<RenderComponent>();
		renderer->SetMesh(wallMesh);
		renderer->SetMaterial(boxMaterial);

		RigidBody::Sptr physics = topWallLeft->Add<RigidBody>(RigidBodyType::Kinematic);
		physics->AddCollider(BoxCollider::Create(glm::vec3(10.0f, 1.0f, 10.0f)));
	}

	GameObject::Sptr topWallRight = scene->CreateGameObject("Top Wall Right" + std::to_string(index));
	{
		topWallRight->SetPostion(glm::vec3(plane->GetPosition().x + 15, plane->GetPosition().y + 25, 10.0f));

		RenderComponent::Sptr renderer = topWallRight->Add<RenderComponent>();
		renderer->SetMesh(wallMesh);
		renderer->SetMaterial(boxMaterial);

		RigidBody::Sptr physics = topWallRight->Add<RigidBody>(RigidBodyType::Kinematic);
		physics->AddCollider(BoxCollider::Create(glm::vec3(10.0f, 1.0f, 10.0f)));
	}

	GameObject::Sptr wallRight = scene->CreateGameObject("Wall Right" + std::to_string(index));
	{
		wallRight->SetPostion(glm::vec3(plane->GetPosition().x + 24.0f, plane->GetPosition().y, 10.0f));
		wallRight->SetRotation(glm::vec3(0.0f, 0.0f, 90.0f));
		wallRight->SetScale(glm::vec3(2.5f, 1.0f, 1.0f));

		RenderComponent::Sptr renderer = wallRight->Add<RenderComponent>();
		renderer->SetMesh(wallMesh);
		renderer->SetMaterial(boxMaterial);

		RigidBody::Sptr physics = wallRight->Add<RigidBody>(RigidBodyType::Kinematic);
		physics->AddCollider(BoxCollider::Create(glm::vec3(25.0f, 1.0f, 10.0f)));
	}

	GameObject::Sptr wallLeft = scene->CreateGameObject("Wall Left" + std::to_string(index));
	{
		wallLeft->SetPostion(glm::vec3(plane->GetPosition().x - 24.0f, plane->GetPosition().y, 10.0f));
		wallLeft->SetRotation(glm::vec3(0.0f, 0.0f, 90.0f));
		wallLeft->SetScale(glm::vec3(2.5f, 1.0f, 1.0f));

		RenderComponent::Sptr renderer = wallLeft->Add<RenderComponent>();
		renderer->SetMesh(wallMesh);
		renderer->SetMaterial(boxMaterial);

		RigidBody::Sptr physics = wallLeft->Add<RigidBody>(RigidBodyType::Kinematic);
		physics->AddCollider(BoxCollider::Create(glm::vec3(25.0f, 1.0f, 10.0f)));
	}

	GameObject::Sptr bottomWallLeft = scene->CreateGameObject("Bottom Wall Left" + std::to_string(index));
	{
		bottomWallLeft->SetPostion(glm::vec3(plane->GetPosition().x - 15, plane->GetPosition().y - 25, 10.0f));

		RenderComponent::Sptr renderer = bottomWallLeft->Add<RenderComponent>();
		renderer->SetMesh(wallMesh);
		renderer->SetMaterial(boxMaterial);

		RigidBody::Sptr physics = bottomWallLeft->Add<RigidBody>(RigidBodyType::Kinematic);
		physics->AddCollider(BoxCollider::Create(glm::vec3(10.0f, 1.0f, 10.0f)));
	}

	GameObject::Sptr bottomWallRight = scene->CreateGameObject("Bottom Wall Right" + std::to_string(index));
	{
		bottomWallRight->SetPostion(glm::vec3(plane->GetPosition().x + 15, plane->GetPosition().y - 25, 10.0f));

		RenderComponent::Sptr renderer = bottomWallRight->Add<RenderComponent>();
		renderer->SetMesh(wallMesh);
		renderer->SetMaterial(boxMaterial);

		RigidBody::Sptr physics = bottomWallRight->Add<RigidBody>(RigidBodyType::Kinematic);
		physics->AddCollider(BoxCollider::Create(glm::vec3(10.0f, 1.0f, 10.0f)));
	}
}

// Create enemies inside the plane
MeshResource::Sptr enemyMesh;
Material::Sptr enemyMaterial;
int enemyAmount = 5;
int enemyCount = 0;
void CreateEnemies(GameObject::Sptr respawnPlane)
{
	enemyAmount = (rand() % 5 + 3) * waveLevel;

	std::vector<GameObject::Sptr> enemy;
	enemy.resize(enemyAmount);

	for (size_t i = 0; i < enemyAmount; i++)
	{
		int randX = rand() % spawnRange + -spawnRange;

		std::random_device device;
		std::default_random_engine engine(device());
		std::uniform_int_distribution<int> distribute(((int)respawnPlane->GetPosition().y - spawnRange), ((int)respawnPlane->GetPosition().y + spawnRange));
		int randY = distribute(engine);

		enemy[i] = scene->CreateGameObject("Enemy" + std::to_string(i));

		enemy[i]->SetPostion(glm::vec3(randX, randY, 1.0f));
		enemy[i]->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));
		enemy[i]->SetScale(glm::vec3(0.5f));

		RenderComponent::Sptr renderer = enemy[i]->Add<RenderComponent>();
		renderer->SetMesh(enemyMesh);
		renderer->SetMaterial(enemyMaterial);

		enemy[i]->SetHealth(20.0f);

		TriggerVolume::Sptr trigger = enemy[i]->Add<TriggerVolume>();
		CylinderCollider::Sptr cylinder = CylinderCollider::Create(glm::vec3(3.0f, 3.0f, 1.0f));
		cylinder->SetPosition(glm::vec3(0.0f, 1.0f, 0.0f));
		cylinder->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));
		trigger->SetFlags(TriggerTypeFlags::Dynamics);
		trigger->AddCollider(cylinder);

		TriggerVolumeEnterBehaviour::Sptr test = enemy[i]->Add<TriggerVolumeEnterBehaviour>();
		test->SetTrigger(false);
	}
}

/// <summary>
/// handles creating or loading the scene
/// </summary>
void CreateScene() 
{
	bool loadScene = false;  
	if (loadScene) 
	{
		ResourceManager::LoadManifest("manifest.json");
		scene = Scene::Load("scene.json");

		scene->Window = window;
		scene->Awake();
	} 
	else 
	{  
		// Create Shaders
		Shader::Sptr basicShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" }, { ShaderPartType::Fragment, "shaders/fragment_shaders/frag_blinn_phong_textured.glsl" }});

		// Create meshes (.obj)
		MeshResource::Sptr goblinMesh = ResourceManager::CreateAsset<MeshResource>("GoblinTest.obj"); enemyMesh = goblinMesh;
		MeshResource::Sptr slimeMesh = ResourceManager::CreateAsset<MeshResource>("Slime.obj");

		// Create textures
		Texture2D::Sptr greenTexture = ResourceManager::CreateAsset<Texture2D>("textures/green.png");
		Texture2D::Sptr groundTexture = ResourceManager::CreateAsset<Texture2D>("textures/ground.png");
		Texture2D::Sptr doorTexture = ResourceManager::CreateAsset<Texture2D>("textures/door.png");
		Texture2D::Sptr wallTexture = ResourceManager::CreateAsset<Texture2D>("textures/wall.png");

		// Create skybox
		TextureCube::Sptr testCubemap = ResourceManager::CreateAsset<TextureCube>("cubemaps/ocean/ocean.jpg");
		Shader::Sptr      skyboxShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/skybox_vert.glsl" }, { ShaderPartType::Fragment, "shaders/fragment_shaders/skybox_frag.glsl" }});

		scene = std::make_shared<Scene>();
		scene->SetSkyboxTexture(testCubemap);
		scene->SetSkyboxShader(skyboxShader);
		scene->SetSkyboxRotation(glm::rotate(MAT4_IDENTITY, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)));

		// Create materials
		Material::Sptr groundMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			groundMaterial->Name = "Ground";
			groundMaterial->Set("u_Material.Diffuse", groundTexture);
			groundMaterial->Set("u_Material.Shininess", 0.1f);
		}

		Material::Sptr doorMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			doorMaterial->Name = "Door";
			doorMaterial->Set("u_Material.Diffuse", doorTexture);
			doorMaterial->Set("u_Material.Shininess", 0.1f);
		}

		Material::Sptr wallMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			wallMaterial->Name = "Wall";
			wallMaterial->Set("u_Material.Diffuse", wallTexture);
			wallMaterial->Set("u_Material.Shininess", 0.1f);
		}

		Material::Sptr greenMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			greenMaterial->Name = "Green";
			greenMaterial->Set("u_Material.Diffuse", greenTexture);
			greenMaterial->Set("u_Material.Shininess", 0.1f);
		} enemyMaterial = greenMaterial;

		// Create lights
		scene->Lights.resize(2);

		scene->Lights[0].Position = glm::vec3(0.0f, 1.0f, 3.0f);
		scene->Lights[0].Color = glm::vec3(0.0f, 0.75f, 0.0f);
		scene->Lights[0].Range = 25.0f;

		scene->Lights[1].Position = glm::vec3(0.0f, 1.0f, 3.0f);
		scene->Lights[1].Color = glm::vec3(1.0f, 1.0f, 1.0f);
		scene->Lights[1].Range = 100.0f;

		// Create custom shapes
		MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
		tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(50.0f), glm::vec2(10.0f)));
		tiledMesh->GenerateMesh();

		MeshResource::Sptr doorMesh = ResourceManager::CreateAsset<MeshResource>();
		doorMesh->AddParam(MeshBuilderParam::CreateCube(ZERO, glm::vec3(10.0f, 1.0f, 10.0f)));
		doorMesh->GenerateMesh();

		MeshResource::Sptr wallMesh = ResourceManager::CreateAsset<MeshResource>();
		wallMesh->AddParam(MeshBuilderParam::CreateCube(ZERO, glm::vec3(20.0f, 2.0f, 20.0f)));
		wallMesh->GenerateMesh();

		// Create camera
		GameObject::Sptr camera = scene->CreateGameObject("Main Camera"); 
		{
			camera->SetPostion(glm::vec3(5.0f));
			camera->LookAt(glm::vec3(0.0f));

			Camera::Sptr cam = camera->Add<Camera>();
			scene->MainCamera = cam;
		}

		// Create game objects
		GameObject::Sptr player = scene->CreateGameObject("Player");
		{
			player->SetPostion(glm::vec3(0.0f, -20.0f, 1.0f));

			player->Add<MovementComponent>();
			player->Add<AbilityComponent>();
			player->SetHealth(100.0f);

			RenderComponent::Sptr renderer = player->Add<RenderComponent>();
			renderer->SetMesh(slimeMesh);
			renderer->SetMaterial(greenMaterial);

			RigidBody::Sptr physics = player->Add<RigidBody>(RigidBodyType::Dynamic);
			physics->AddCollider(ConvexMeshCollider::Create());
			physics->SetAngularFactor(glm::vec3(0.0f));

			TriggerVolume::Sptr trigger = player->Add<TriggerVolume>();
			CylinderCollider::Sptr cylinder = CylinderCollider::Create(glm::vec3(1.0f, 1.0f, 1.0f));
			cylinder->SetPosition(glm::vec3(0.0f, 1.0f, 0.0f));
			trigger->SetFlags(TriggerTypeFlags::Kinematics | TriggerTypeFlags::Statics);
			trigger->AddCollider(cylinder);

			TriggerVolumeEnterBehaviour::Sptr test = player->Add<TriggerVolumeEnterBehaviour>();
			test->SetTrigger(false);
		}

		GameObject::Sptr plane1 = scene->CreateGameObject("Plane1");
		{
			RenderComponent::Sptr renderer = plane1->Add<RenderComponent>();
			renderer->SetMesh(tiledMesh);
			renderer->SetMaterial(groundMaterial);

			RigidBody::Sptr physics = plane1->Add<RigidBody>(RigidBodyType::Kinematic);
			physics->AddCollider(BoxCollider::Create(glm::vec3(25.0f, 25.0f, 1.0f)))->SetPosition({ 0, 0, -1 });

			TriggerVolume::Sptr volume = plane1->Add<TriggerVolume>();

			BoxCollider::Sptr box = BoxCollider::Create(glm::vec3(22.0f, 1.0f, 1.0f));
			box->SetPosition(glm::vec3(0.0f, -20.0f, 3.0f));
			volume->SetFlags(TriggerTypeFlags::Dynamics);
			volume->AddCollider(box);

			TriggerVolumeEnterBehaviour::Sptr test = plane1->Add<TriggerVolumeEnterBehaviour>();
			test->SetTrigger(false);
		}

		GameObject::Sptr door1 = scene->CreateGameObject("Door1");
		{
			door1->SetPostion(glm::vec3(0.0f, plane1->GetPosition().y + 25, 5.0f));

			RenderComponent::Sptr renderer = door1->Add<RenderComponent>();
			renderer->SetMesh(doorMesh);
			renderer->SetMaterial(doorMaterial);

			RigidBody::Sptr physics = door1->Add<RigidBody>(RigidBodyType::Kinematic);
			physics->AddCollider(BoxCollider::Create(glm::vec3(5.0f, 1.0f, 5.0f)));
		}

		CreateWalls(1, plane1, wallMesh, wallMaterial);

		GameObject::Sptr plane2 = scene->CreateGameObject("Plane2");
		{
			plane2->SetPostion(plane1->GetPosition() + glm::vec3(0, planeDifference, 0));

			RenderComponent::Sptr renderer = plane2->Add<RenderComponent>();
			renderer->SetMesh(tiledMesh);
			renderer->SetMaterial(groundMaterial);

			RigidBody::Sptr physics = plane2->Add<RigidBody>(RigidBodyType::Kinematic);
			physics->AddCollider(BoxCollider::Create(glm::vec3(25.0f, 25.0f, 1.0f)))->SetPosition({ 0, 0, -1 });

			TriggerVolume::Sptr volume = plane2->Add<TriggerVolume>();

			BoxCollider::Sptr box = BoxCollider::Create(glm::vec3(22.0f, 1.0f, 1.0f));
			box->SetPosition(glm::vec3(0.0f, -20.0f, 3.0f));
			volume->SetFlags(TriggerTypeFlags::Dynamics);
			volume->AddCollider(box);

			TriggerVolumeEnterBehaviour::Sptr test = plane2->Add<TriggerVolumeEnterBehaviour>();
			test->SetTrigger(false);
		}

		GameObject::Sptr door2 = scene->CreateGameObject("Door2");
		{
			door2->SetPostion(glm::vec3(0.0f, plane2->GetPosition().y + 25, 5.0f));

			RenderComponent::Sptr renderer = door2->Add<RenderComponent>();
			renderer->SetMesh(doorMesh);
			renderer->SetMaterial(doorMaterial);

			RigidBody::Sptr physics = door2->Add<RigidBody>(RigidBodyType::Kinematic);
			physics->AddCollider(BoxCollider::Create(glm::vec3(5.0f, 1.0f, 5.0f)));
		}

		CreateWalls(2, plane2, wallMesh, wallMaterial);

		GameObject::Sptr backDoor = scene->CreateGameObject("Back Door");
		{
			backDoor->SetPostion(glm::vec3(0.0f, plane1->GetPosition().y - 25, 5.0f));

			RenderComponent::Sptr renderer = backDoor->Add<RenderComponent>();
			renderer->SetMesh(doorMesh);
			renderer->SetMaterial(doorMaterial);

			RigidBody::Sptr physics = backDoor->Add<RigidBody>(RigidBodyType::Kinematic);
			physics->AddCollider(BoxCollider::Create(glm::vec3(5.0f, 1.0f, 5.0f)));
		}

		CreateEnemies(plane1);

		// Create UI panels
		GameObject::Sptr startPanel = scene->CreateGameObject("Start Panel");
		{
			RectTransform::Sptr transform = startPanel->Add<RectTransform>();
			transform->SetMin({ -100, -100 });
			transform->SetMax({ 200, 200 });
			transform->SetSize({ windowSize.x, windowSize.y });

			GuiPanel::Sptr panel = startPanel->Add<GuiPanel>();
			panel->SetColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

			Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 32.0f);
			font->Bake();

			const std::string newText = "Press Space To Start";
			GuiText::Sptr text = startPanel->Add<GuiText>();
			text->SetText(newText);
			text->SetFont(font);
			text->SetColor(glm::vec4(1.0f));
		}

		GameObject::Sptr wavePanel = scene->CreateGameObject("Wave Panel");
		{
			RectTransform::Sptr transform = wavePanel->Add<RectTransform>();
			transform->SetMin({ -100, -100 });
			transform->SetMax({ 200, 200 });

			GuiPanel::Sptr panel = wavePanel->Add<GuiPanel>();
			panel->SetColor(glm::vec4(0.0f));

			Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 32.0f);
			font->Bake();

			const std::string newText = "Wave " + std::to_string(waveLevel);
			GuiText::Sptr text = wavePanel->Add<GuiText>();
			text->SetText(newText);
			text->SetFont(font);
			text->SetColor(glm::vec4(1.0f));
			//text->SetTextScale(2.0f);
		}

		GameObject::Sptr healthBarBack = scene->CreateGameObject("Health Bar Back");
		{
			RectTransform::Sptr transform = healthBarBack->Add<RectTransform>();
			transform->SetPosition(glm::vec2(windowSize.x / 2, 900.0f));
			transform->SetMin({ 10, 5 });
			transform->SetMax({ 100, 50 });
			transform->SetSize(glm::vec2(100.0f, 10.0f));

			GuiPanel::Sptr panel = healthBarBack->Add<GuiPanel>();
			panel->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
		}

		GameObject::Sptr healthBar = scene->CreateGameObject("Health Bar");
		{
			RectTransform::Sptr transform = healthBar->Add<RectTransform>();
			transform->SetPosition(glm::vec2(windowSize.x / 2, 900.0f));
			transform->SetMin({ 10, 5 });
			transform->SetMax({ 100, 50 });

			GuiPanel::Sptr panel = healthBar->Add<GuiPanel>();
			panel->SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

			//Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 24.0f);
			//font->Bake();

			//const std::string newText = std::to_string(player->GetHealth());
			//GuiText::Sptr text = healthBar->Add<GuiText>();
			//text->SetText(newText);
			//text->SetFont(font);
			//text->SetColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		}

		GameObject::Sptr healthText = scene->CreateGameObject("Health Text");
		{
			RectTransform::Sptr transform = healthText->Add<RectTransform>();
			transform->SetPosition(glm::vec2(windowSize.x / 2, 900.0f));
			transform->SetMin({ 10, 5 });
			transform->SetMax({ 100, 50 });

			GuiPanel::Sptr panel = healthText->Add<GuiPanel>();
			panel->SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));

			Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 24.0f);
			font->Bake();

			const std::string newText = std::to_string(player->GetHealth());
			GuiText::Sptr text = healthText->Add<GuiText>();
			text->SetText(newText);
			text->SetFont(font);
			text->SetColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
		}

		GuiBatcher::SetDefaultTexture(ResourceManager::CreateAsset<Texture2D>("textures/ui-sprite.png"));
		GuiBatcher::SetDefaultBorderRadius(8);

		scene->Window = window;
		scene->Awake();

		ResourceManager::SaveManifest("manifest.json");

		scene->Save("scene.json");
	}
}

int main() 
{
	Logger::Init();

	if (!initGLFW()) return 1;
	if (!initGLAD()) return 1;

	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(GlDebugMessage, nullptr);

	ImGuiHelper::Init(window);

	ResourceManager::Init();

	ResourceManager::RegisterType<Texture2D>();
	ResourceManager::RegisterType<TextureCube>();
	ResourceManager::RegisterType<Shader>();
	ResourceManager::RegisterType<Material>();
	ResourceManager::RegisterType<MeshResource>();

	ComponentManager::RegisterType<Camera>();
	ComponentManager::RegisterType<RenderComponent>();
	ComponentManager::RegisterType<RigidBody>();
	ComponentManager::RegisterType<TriggerVolume>();
	ComponentManager::RegisterType<RotatingBehaviour>();
	ComponentManager::RegisterType<JumpBehaviour>();
	ComponentManager::RegisterType<MaterialSwapBehaviour>();
	ComponentManager::RegisterType<TriggerVolumeEnterBehaviour>();
	ComponentManager::RegisterType<SimpleCameraControl>();
	ComponentManager::RegisterType<AbilityComponent>();
	ComponentManager::RegisterType<MovementComponent>();

	ComponentManager::RegisterType<RectTransform>();
	ComponentManager::RegisterType<GuiPanel>();
	ComponentManager::RegisterType<GuiText>();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

	struct FrameLevelUniforms 
	{
		glm::mat4 u_View;
		glm::mat4 u_Projection;
		glm::mat4 u_ViewProjection;
		glm::vec4 u_CameraPos;
		float u_Time;
	};
	UniformBuffer<FrameLevelUniforms>::Sptr frameUniforms = std::make_shared<UniformBuffer<FrameLevelUniforms>>(BufferUsage::DynamicDraw);
	const int FRAME_UBO_BINDING = 0;

	struct InstanceLevelUniforms 
	{
		glm::mat4 u_ModelViewProjection;
		glm::mat4 u_Model;
		glm::mat4 u_NormalMatrix;
	};
	UniformBuffer<InstanceLevelUniforms>::Sptr instanceUniforms = std::make_shared<UniformBuffer<InstanceLevelUniforms>>(BufferUsage::DynamicDraw);
	const int INSTANCE_UBO_BINDING = 1;

	CreateScene();

	std::string scenePath = "scene.json"; 
	scenePath.reserve(256); 

	double lastFrame = glfwGetTime();

	BulletDebugMode physicsDebugMode = BulletDebugMode::None;
	float playbackSpeed = 1.0f;
	bool isPaused = false;

	nlohmann::json editorSceneState;

////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////// GAME LOOP //////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
	
	while (!glfwWindowShouldClose(window)) 
	{
		glfwPollEvents();
		ImGuiHelper::StartFrame();

		double thisFrame = glfwGetTime();
		float dt = static_cast<float>(thisFrame - lastFrame);

		if (!scene->IsPlaying && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) scene->IsPlaying = !scene->IsPlaying;

		GameObject::Sptr player = scene->FindObjectByName("Player");

		GameObject::Sptr startPanel = scene->FindObjectByName("Start Panel");
		startPanel->Get<RectTransform>()->SetSize({ windowSize.x, windowSize.y });
		startPanel->Get<RectTransform>()->SetPosition({ windowSize.x / 2, windowSize.y / 2 });
		if (scene->IsPlaying)
		{
			startPanel->Get<GuiPanel>()->SetColor(glm::vec4(0.0f));
			startPanel->Get<GuiText>()->SetText("");
		}
		if (scene->IsPlaying && player->GetHealth() <= 0)
		{
			startPanel->Get<GuiPanel>()->SetColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			startPanel->Get<GuiText>()->SetText("Game Over");
			playbackSpeed = 0.0f;
		}

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) { isPaused = !isPaused; glfwWaitEventsTimeout(0.25f); }
		playbackSpeed = isPaused ? 0.0f : 1.0f;
		if (isPaused && scene->IsPlaying) startPanel->Get<GuiText>()->SetText("Paused");
		if (!isPaused && scene->IsPlaying && player->GetHealth() > 0) startPanel->Get<GuiText>()->SetText("");

		GameObject::Sptr plane1 = scene->FindObjectByName("Plane1");
		GameObject::Sptr topWallLeft1 = scene->FindObjectByName("Top Wall Left1");
		GameObject::Sptr topWallRight1 = scene->FindObjectByName("Top Wall Right1");
		GameObject::Sptr wallRight1 = scene->FindObjectByName("Wall Right1");
		GameObject::Sptr wallLeft1 = scene->FindObjectByName("Wall Left1");
		GameObject::Sptr bottomWallLeft1 = scene->FindObjectByName("Bottom Wall Left1");
		GameObject::Sptr bottomWallRight1 = scene->FindObjectByName("Bottom Wall Right1");
		GameObject::Sptr door1 = scene->FindObjectByName("Door1");

		GameObject::Sptr plane2 = scene->FindObjectByName("Plane2");
		GameObject::Sptr topWallLeft2 = scene->FindObjectByName("Top Wall Left2");
		GameObject::Sptr topWallRight2 = scene->FindObjectByName("Top Wall Right2");
		GameObject::Sptr wallRight2 = scene->FindObjectByName("Wall Right2");
		GameObject::Sptr wallLeft2 = scene->FindObjectByName("Wall Left2");
		GameObject::Sptr bottomWallLeft2 = scene->FindObjectByName("Bottom Wall Left2");
		GameObject::Sptr bottomWallRight2 = scene->FindObjectByName("Bottom Wall Right2");
		GameObject::Sptr door2 = scene->FindObjectByName("Door2");

		GameObject::Sptr backDoor = scene->FindObjectByName("Back Door");

		if (plane1->Get<TriggerVolumeEnterBehaviour>()->GetTrigger() && planeSwitch)
		{
			player->SetScale(glm::vec3(1.0f));
			plane2->SetPostion(plane1->GetPosition() + glm::vec3(0, planeDifference, 0));

			CreateEnemies(plane1);

			door1->SetPostion(glm::vec3(0.0f, plane1->GetPosition().y + 25.0f, 5.0f));
			topWallLeft1->SetPostion(glm::vec3(plane1->GetPosition().x - 15, plane1->GetPosition().y + 25, 10.0f));
			topWallRight1->SetPostion(glm::vec3(plane1->GetPosition().x + 15, plane1->GetPosition().y + 25, 10.0f));
			wallRight1->SetPostion(glm::vec3(plane1->GetPosition().x + 24.0f, plane1->GetPosition().y, 10.0f));
			wallLeft1->SetPostion(glm::vec3(plane1->GetPosition().x - 24.0f, plane1->GetPosition().y, 10.0f));
			bottomWallLeft1->SetPostion(glm::vec3(plane1->GetPosition().x - 15, plane1->GetPosition().y - 25, 10.0f));
			bottomWallRight1->SetPostion(glm::vec3(plane1->GetPosition().x + 15, plane1->GetPosition().y - 25, 10.0f));

			door2->SetPostion(glm::vec3(0.0f, plane2->GetPosition().y + 25.0f, 5.0f));
			topWallLeft2->SetPostion(glm::vec3(plane2->GetPosition().x - 15, plane2->GetPosition().y + 25, 10.0f));
			topWallRight2->SetPostion(glm::vec3(plane2->GetPosition().x + 15, plane2->GetPosition().y + 25, 10.0f));
			wallRight2->SetPostion(glm::vec3(plane2->GetPosition().x + 24.0f, plane2->GetPosition().y, 10.0f));
			wallLeft2->SetPostion(glm::vec3(plane2->GetPosition().x - 24.0f, plane2->GetPosition().y, 10.0f));
			bottomWallLeft2->SetPostion(glm::vec3(plane2->GetPosition().x - 15, plane2->GetPosition().y - 25, 10.0f));
			bottomWallRight2->SetPostion(glm::vec3(plane2->GetPosition().x + 15, plane2->GetPosition().y - 25, 10.0f));

			backDoor->SetPostion(glm::vec3(0.0f, plane1->GetPosition().y - 25, 5.0f));

			waveLevel++;
			t = 0.0f;
			planeSwitch = !planeSwitch;
		}
		if (plane2->Get<TriggerVolumeEnterBehaviour>()->GetTrigger() && !planeSwitch)
		{
			player->SetScale(glm::vec3(1.0f));
			plane1->SetPostion(plane2->GetPosition() + glm::vec3(0, planeDifference, 0));

			CreateEnemies(plane2);

			door1->SetPostion(glm::vec3(0.0f, plane1->GetPosition().y + 25.0f, 5.0f));
			topWallLeft1->SetPostion(glm::vec3(plane1->GetPosition().x - 15, plane1->GetPosition().y + 25, 10.0f));
			topWallRight1->SetPostion(glm::vec3(plane1->GetPosition().x + 15, plane1->GetPosition().y + 25, 10.0f));
			wallRight1->SetPostion(glm::vec3(plane1->GetPosition().x + 24.0f, plane1->GetPosition().y, 10.0f));
			wallLeft1->SetPostion(glm::vec3(plane1->GetPosition().x - 24.0f, plane1->GetPosition().y, 10.0f));
			bottomWallLeft1->SetPostion(glm::vec3(plane1->GetPosition().x - 15, plane1->GetPosition().y - 25, 10.0f));
			bottomWallRight1->SetPostion(glm::vec3(plane1->GetPosition().x + 15, plane1->GetPosition().y - 25, 10.0f));

			door2->SetPostion(glm::vec3(0.0f, plane2->GetPosition().y + 25.0f, 5.0f));
			topWallLeft2->SetPostion(glm::vec3(plane2->GetPosition().x - 15, plane2->GetPosition().y + 25, 10.0f));
			topWallRight2->SetPostion(glm::vec3(plane2->GetPosition().x + 15, plane2->GetPosition().y + 25, 10.0f));
			wallRight2->SetPostion(glm::vec3(plane2->GetPosition().x + 24.0f, plane2->GetPosition().y, 10.0f));
			wallLeft2->SetPostion(glm::vec3(plane2->GetPosition().x - 24.0f, plane2->GetPosition().y, 10.0f));
			bottomWallLeft2->SetPostion(glm::vec3(plane2->GetPosition().x - 15, plane2->GetPosition().y - 25, 10.0f));
			bottomWallRight2->SetPostion(glm::vec3(plane2->GetPosition().x + 15, plane2->GetPosition().y - 25, 10.0f));

			backDoor->SetPostion(glm::vec3(0.0f, plane2->GetPosition().y - 25, 5.0f));

			waveLevel++;
			t = 0.0f;
			planeSwitch = !planeSwitch;
		}

		std::vector<GameObject::Sptr> enemy;
		enemy.resize(enemyAmount);
		enemyCount = enemyAmount;

		for (size_t i = 0; i < enemyAmount; i++)
		{
			enemy[i] = scene->FindObjectByName("Enemy" + std::to_string(i));
			if (enemy[i] != nullptr && enemy[i]->Get<TriggerVolumeEnterBehaviour>() != nullptr)
			{
				if (scene->IsPlaying && playbackSpeed == 1.0f)
				{
					UseAbility(player, enemy[i], glfwGetTime());
					EnemySteeringBehaviour(player, enemy[i], dt);
					TakeDamage(player, enemy[i], glfwGetTime());
				}
			}

			if (scene->FindObjectByName("Enemy" + std::to_string(i)) == nullptr)
			{
				enemyCount--;
				if (enemyCount == 0)
				{
					if (t < 1.0f) { t += 0.01;  }
					door1->SetPostion(LERP(glm::vec3(0.0f, plane1->GetPosition().y + 25.0f, 5.0f), glm::vec3(0.0f, plane1->GetPosition().y + 25.0f, 10.0f), t));
					door2->SetPostion(LERP(glm::vec3(0.0f, plane2->GetPosition().y + 25.0f, 5.0f), glm::vec3(0.0f, plane2->GetPosition().y + 25.0f, 10.0f), t));
				}
			}
		}

		const std::string newText = "Wave " + std::to_string(waveLevel);
		GameObject::Sptr wavePanel = scene->FindObjectByName("Wave Panel");
		if (!scene->IsPlaying) wavePanel->Get<GuiText>()->SetText("");
		else wavePanel->Get<GuiText>()->SetText(newText);

		GameObject::Sptr healthBarBack = scene->FindObjectByName("Health Bar Back");
		GameObject::Sptr healthBar = scene->FindObjectByName("Health Bar");
		RectTransform::Sptr rect = healthBar->Get<RectTransform>();
		rect->SetPosition(glm::vec2(windowSize.x / 2, windowSize.y - 50.0f));
		rect->SetSize(glm::vec2(player->GetHealth(), 10.0f));
		GuiPanel::Sptr panel = healthBar->Get<GuiPanel>();
		if (player->GetHealth() >= 50) { panel->SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)); healthBarBack->Get<GuiPanel>()->SetColor(glm::vec4(1.0f)); }
		if (player->GetHealth() < 50 && player->GetHealth() >= 25) panel->SetColor(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
		if (player->GetHealth() < 25 && player->GetHealth() > 0) panel->SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
		if (!scene->IsPlaying || player->GetHealth() <= 0) 
		{ 
			panel->SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)); 
			healthBarBack->Get<GuiPanel>()->SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 0.0f)); 
		}
		
		GameObject::Sptr healthText = scene->FindObjectByName("Health Text");
		healthText->Get<RectTransform>()->SetPosition(glm::vec2(windowSize.x / 2, windowSize.y - 40.0f));
		int healthUI = player->GetHealth();
		if (!scene->IsPlaying) healthText->Get<GuiText>()->SetText("");
		else healthText->Get<GuiText>()->SetText(std::to_string(healthUI));

		healthBarBack->Get<RectTransform>()->SetPosition(glm::vec2(windowSize.x / 2, windowSize.y - 50.0f));
		
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		dt *= playbackSpeed;

		scene->Lights[0].Position = player->GetPosition();
		scene->Lights[1].Position = glm::vec3(player->GetPosition().x, player->GetPosition().y, player->GetPosition().z + 20.0f);
		scene->SetupShaderAndLights();

		scene->Update(dt);

		Camera::Sptr camera = scene->MainCamera;

		GameObject* cam = camera->GetGameObject();
		TopDownCamera(cam, player);

		glm::mat4 viewProj = camera->GetViewProjection();
		DebugDrawer::Get().SetViewProjection(viewProj);

		scene->DoPhysics(dt);
		
		Material::Sptr currentMat = nullptr;
		Shader::Sptr shader = nullptr;

		TextureCube::Sptr environment = scene->GetSkyboxTexture();
		if (environment) environment->Bind(0); 

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		scene->PreRender();
		frameUniforms->Bind(FRAME_UBO_BINDING);
		instanceUniforms->Bind(INSTANCE_UBO_BINDING);

		auto& frameData = frameUniforms->GetData();
		frameData.u_Projection = camera->GetProjection();
		frameData.u_View = camera->GetView();
		frameData.u_ViewProjection = camera->GetViewProjection();
		frameData.u_CameraPos = glm::vec4(camera->GetGameObject()->GetPosition(), 1.0f);
		frameData.u_Time = static_cast<float>(thisFrame);
		frameUniforms->Update();

		ComponentManager::Each<RenderComponent>([&](const RenderComponent::Sptr& renderable) 
		{
			if (renderable->GetMesh() == nullptr) 
			{ 
				return;
			}

			if (renderable->GetMaterial() == nullptr) 
			{
				if (scene->DefaultMaterial != nullptr) 
				{
					renderable->SetMaterial(scene->DefaultMaterial);
				} 
				else 
				{
					return;
				}
			}

			if (renderable->GetMaterial() != currentMat) 
			{
				currentMat = renderable->GetMaterial();
				shader = currentMat->GetShader();

				shader->Bind();
				currentMat->Apply();
			}

			GameObject* object = renderable->GetGameObject();
			 
			auto& instanceData = instanceUniforms->GetData();
			instanceData.u_Model = object->GetTransform();
			instanceData.u_ModelViewProjection = viewProj * object->GetTransform();
			instanceData.u_NormalMatrix = glm::mat3(glm::transpose(glm::inverse(object->GetTransform())));
			instanceUniforms->Update();  

			renderable->GetMesh()->Draw();
		});

		scene->DrawSkybox();

		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glEnable(GL_SCISSOR_TEST);

		glm::mat4 proj = glm::ortho(0.0f, (float)windowSize.x, (float)windowSize.y, 0.0f, -1.0f, 1.0f);
		GuiBatcher::SetProjection(proj);

		scene->RenderGUI();

		GuiBatcher::Flush();

		glDisable(GL_BLEND);
		glDisable(GL_SCISSOR_TEST);
		glDepthMask(GL_TRUE);

		VertexArrayObject::Unbind();

		lastFrame = thisFrame;
		ImGuiHelper::EndFrame();
		InputEngine::EndFrame();
		glfwSwapBuffers(window);
	}

	ImGuiHelper::Cleanup();
	ResourceManager::Cleanup();
	Logger::Uninitialize();
	return 0;
}