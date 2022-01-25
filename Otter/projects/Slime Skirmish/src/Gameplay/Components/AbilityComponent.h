#pragma once
#include "IComponent.h"
#include "Gameplay/Physics/RigidBody.h"


class AbilityComponent : public Gameplay::IComponent
{
public:
	typedef std::shared_ptr<AbilityComponent> Sptr;

	enum AbilityType
	{
		None,
		Absorb,
		Attack
	};

	AbilityComponent(AbilityType type = AbilityType::None);

	void SetType(AbilityType newType);
	AbilityType GetType() const;

	void Use();

	virtual void RenderImGui() override;
	virtual nlohmann::json ToJson() const override;
	static AbilityComponent::Sptr FromJson(const nlohmann::json& blob);
	MAKE_TYPENAME(AbilityComponent);

private:
	AbilityType _type;


};

