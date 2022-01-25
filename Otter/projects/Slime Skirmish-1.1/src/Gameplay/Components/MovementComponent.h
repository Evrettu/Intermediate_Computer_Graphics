#pragma once
#include "IComponent.h"
#include "Gameplay/Physics/RigidBody.h"

/// <summary>
/// A simple behaviour that applies an impulse along the Z axis to the 
/// rigidbody of the parent when the space key is pressed
/// </summary>
class MovementComponent : public Gameplay::IComponent {
public:
	typedef std::shared_ptr<MovementComponent> Sptr;

	MovementComponent();
	virtual ~MovementComponent();

	virtual void Awake() override;
	virtual void Update(float deltaTime) override;

public:
	virtual void RenderImGui() override;
	MAKE_TYPENAME(MovementComponent);
	virtual nlohmann::json ToJson() const override;
	static MovementComponent::Sptr FromJson(const nlohmann::json& blob);

protected:
	float _impulse;
	int rotation = 0;

	bool _isPressed = false;
	Gameplay::Physics::RigidBody::Sptr _body;
};