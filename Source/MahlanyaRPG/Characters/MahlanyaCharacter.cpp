// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#include "Characters/MahlanyaCharacter.h"
#include "Characters/MahlanyaAbilityComponent.h"
#include "Characters/MahlanyaEffectsComponent.h"
#include "SwaziKineticLocomotion.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

AMahlanyaCharacter::AMahlanyaCharacter()
{
	// Rotate character toward movement direction, not camera
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate               = FRotator(0.f, 500.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed               = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed         = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Third-person camera rig
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength         = 400.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Simulation components
	AbilityComponent  = CreateDefaultSubobject<UMahlanyaAbilityComponent>(TEXT("AbilityComponent"));
	EffectsComponent  = CreateDefaultSubobject<UMahlanyaEffectsComponent>(TEXT("EffectsComponent"));
	KineticLocomotion = CreateDefaultSubobject<USwaziKineticLocomotion>(TEXT("KineticLocomotion"));
}

void AMahlanyaCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AMahlanyaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (JumpAction)
		{
			EIC->BindAction(JumpAction, ETriggerEvent::Started,   this, &ACharacter::Jump);
			EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}
		if (MoveAction)
			EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMahlanyaCharacter::Move);
		if (LookAction)
			EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMahlanyaCharacter::Look);
	}
}

void AMahlanyaCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Controller && (Axis.X != 0.f || Axis.Y != 0.f))
	{
		const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
	}
}

void AMahlanyaCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Controller)
	{
		AddControllerYawInput(Axis.X);
		AddControllerPitchInput(Axis.Y);
	}
}
