#include "AbilityComponent.h"
#include <GLFW/glfw3.h>
#include "Gameplay/GameObject.h"
#include "Gameplay/Scene.h"
#include "Utils/ImGuiHelper.h"

AbilityComponent::AbilityComponent(AbilityType type) : IComponent()
{
	_type = type;
}

void AbilityComponent::SetType(AbilityType newType)
{
	_type = newType;
}

AbilityComponent::AbilityType AbilityComponent::GetType() const
{
	return _type;
}

void AbilityComponent::Use()
{
	//if (_type == AbilityType::None)
	//{
	//	std::cout << "CANNOT ABSORB" << std::endl;
	//}
	//if (_type == AbilityType::Absorb)
	//{
	//	std::cout << "ABSORB" << std::endl;
	//}

	//if (_type == AbilityType::Attack)
	//{
	//	std::cout << "ATTACK" << std::endl;
	//}
}

void AbilityComponent::RenderImGui()
{
}

nlohmann::json AbilityComponent::ToJson() const
{
	return { { "ability_type", _type } };
}

AbilityComponent::Sptr AbilityComponent::FromJson(const nlohmann::json& blob)
{
	AbilityComponent::Sptr result = std::make_shared<AbilityComponent>();
	result->_type = blob["ability_type"];
	return result;
}