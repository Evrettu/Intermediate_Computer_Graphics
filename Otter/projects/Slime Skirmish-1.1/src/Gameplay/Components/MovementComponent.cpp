#include "Gameplay/Components/MovementComponent.h"
#include <GLFW/glfw3.h>
#include "Gameplay/GameObject.h"
#include "Gameplay/Scene.h"
#include "Utils/ImGuiHelper.h"

void MovementComponent::Awake()
{
	_body = GetComponent<Gameplay::Physics::RigidBody>();
	if (_body == nullptr) 
	{
		IsEnabled = false;
	}
}

void MovementComponent::RenderImGui() 
{
	LABEL_LEFT(ImGui::DragFloat, "Impulse", &_impulse, 1.0f);
}

nlohmann::json MovementComponent::ToJson() const 
{
	return 
	{
		{ "impulse", _impulse }
	};
}

MovementComponent::MovementComponent() : IComponent(), _impulse(10.0f)
{ 

}

MovementComponent::~MovementComponent() = default;

MovementComponent::Sptr MovementComponent::FromJson(const nlohmann::json & blob) 
{
	MovementComponent::Sptr result = std::make_shared<MovementComponent>();
	result->_impulse = blob["impulse"];
	return result;
}

void MovementComponent::Update(float deltaTime)
{
	bool pressed = glfwGetKey(GetGameObject()->GetScene()->Window, GLFW_KEY_W);
	if (pressed)
	{
		if (_isPressed == false)
		{
			_body->SetLinearVelocity(glm::vec3(0.0f, _impulse, 0.0f));
			rotation = 1;
		}
		_isPressed = pressed;
	}
	else 
	{
		_isPressed = false;
	}

	bool pressed1 = glfwGetKey(GetGameObject()->GetScene()->Window, GLFW_KEY_S);
	if (pressed1)
	{
		if (_isPressed == false) 
		{
			_body->SetLinearVelocity(glm::vec3(0.0f, _impulse * -1.0f, 0.0f));
			rotation = 2;
		}
		_isPressed = pressed1;
	}
	else 
	{
		_isPressed = false;
	}

	bool pressed2 = glfwGetKey(GetGameObject()->GetScene()->Window, GLFW_KEY_A);
	if (pressed2) 
	{
		if (_isPressed == false) 
		{
			_body->SetLinearVelocity(glm::vec3(_impulse * -1.0f, 0.0f, 0.0f));
			rotation = 3;
		}
		_isPressed = pressed2;
	}
	else 
	{
		_isPressed = false;
	}

	bool pressed3 = glfwGetKey(GetGameObject()->GetScene()->Window, GLFW_KEY_D);
	if (pressed3) 
	{
		if (_isPressed == false) 
		{
			_body->SetLinearVelocity(glm::vec3(_impulse, 0.0f, 0.0f));
			rotation = 4;
		}
		_isPressed = pressed3;
	}
	else 
	{
		_isPressed = false;
	}
}

