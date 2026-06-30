// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "MahlanyaCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UMahlanyaAbilityComponent;
class UMahlanyaEffectsComponent;
class USwaziKineticLocomotion;
class UInputMappingContext;
class UInputAction;

UCLASS()
class MAHLANYARPG_API AMahlanyaCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMahlanyaCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	// Third-person camera
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// Simulation components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mahlanya|Abilities")
	TObjectPtr<UMahlanyaAbilityComponent> AbilityComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mahlanya|Abilities")
	TObjectPtr<UMahlanyaEffectsComponent> EffectsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mahlanya|Locomotion")
	TObjectPtr<USwaziKineticLocomotion> KineticLocomotion;

	// Enhanced Input — assign in Blueprint subclass defaults
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

public:
	FORCEINLINE USpringArmComponent*        GetCameraBoom()        const { return CameraBoom; }
	FORCEINLINE UCameraComponent*           GetFollowCamera()      const { return FollowCamera; }
	FORCEINLINE UMahlanyaAbilityComponent*  GetAbilityComponent()  const { return AbilityComponent; }
	FORCEINLINE USwaziKineticLocomotion*    GetKineticLocomotion() const { return KineticLocomotion; }
};
