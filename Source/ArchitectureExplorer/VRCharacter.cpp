// Fill out your copyright notice in the Description page of Project Settings.


#include "VRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"

// Sets default values
AVRCharacter::AVRCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	
	VRRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VR Root"));
	VRRoot->SetupAttachment(GetRootComponent());

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VRRoot);

	DestinationMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Destination Marker"));
	DestinationMarker->SetupAttachment(GetRootComponent());
	

}

// Called when the game starts or when spawned
void AVRCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AVRCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector NewCameraOffset = Camera->GetComponentLocation() - GetActorLocation();
	NewCameraOffset.Z = 0;
	AddActorWorldOffset(NewCameraOffset);
	VRRoot->AddWorldOffset(-NewCameraOffset);

	UpdateDestinationMarker();
}

// Called to bind functionality to input
void AVRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"),this,&AVRCharacter::MoveUp);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"),this,&AVRCharacter::MoveRight);

	PlayerInputComponent->BindAction(TEXT("Trigger"),EInputEvent::IE_Pressed,this,&AVRCharacter::BeginTeleport);

}

void AVRCharacter::MoveUp(float AxisValue)
{
	AddMovementInput(Camera->GetForwardVector() * AxisValue);
}

void AVRCharacter::MoveRight(float AxisValue)
{
	AddMovementInput(Camera->GetRightVector() * AxisValue);
}

void AVRCharacter::UpdateDestinationMarker()
{
	FHitResult Hit;
	FVector Start = Camera->GetComponentLocation();
	FVector End = Start + Camera->GetForwardVector() * DestinationMarkerRange;

	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECollisionChannel::ECC_Visibility);

	if (bHit){
 		DestinationMarker->SetWorldLocation(Hit.Location);
		DestinationMarker->SetHiddenInGame(false);
	}else{
		DestinationMarker->SetHiddenInGame(true);
	}
}

void AVRCharacter::BeginTeleport()
{
	//Get Player Controller
	APlayerController* Player = Cast<APlayerController>(GetController());
	
	if (Player != nullptr){
		//Fade Camera to Black
		Player->PlayerCameraManager->StartCameraFade(0,1,FadeTime,FLinearColor::Black,false,true);

		//Set Timer for FadeTime seconds
		FTimerHandle FadeTimer;
		GetWorldTimerManager().SetTimer(FadeTimer,this,&AVRCharacter::FinishTeleport,FadeTime);
	}


}

void AVRCharacter::FinishTeleport()
{
	APlayerController* Player = Cast<APlayerController>(GetController());
	
	if (Player != nullptr){

		//Teleport to Location
		SetActorLocation(DestinationMarker->GetComponentLocation() + GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

		//Fade Back In
		Player->PlayerCameraManager->StartCameraFade(1,0,FadeTime,FLinearColor::Black,false,true);
	}
	
}

