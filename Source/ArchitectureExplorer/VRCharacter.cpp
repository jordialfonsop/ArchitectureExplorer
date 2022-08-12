// Fill out your copyright notice in the Description page of Project Settings.


#include "VRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SplineComponent.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MotionControllerComponent.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AVRCharacter::AVRCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	
	VRRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VR Root"));
	VRRoot->SetupAttachment(GetRootComponent());

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VRRoot);

	LeftController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("Left Controller"));
	LeftController->SetupAttachment(VRRoot);
	LeftController->SetTrackingSource(EControllerHand::Left);
	LeftController->SetDisplayModelSource(TEXT("OculusHMD"));
	LeftController->SetShowDeviceModel(true);

	RightController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("Right Controller"));
	RightController->SetupAttachment(VRRoot);
	RightController->SetTrackingSource(EControllerHand::Right);
	RightController->SetDisplayModelSource(TEXT("OculusHMD"));
	RightController->SetShowDeviceModel(true);

	TeleportPath = CreateDefaultSubobject<USplineComponent>(TEXT("Teleport Path"));
	TeleportPath->SetupAttachment(RightController);

	DestinationMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Destination Marker"));
	DestinationMarker->SetupAttachment(GetRootComponent());

	Blinker = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Blinker"));
	Blinker->SetupAttachment(GetRootComponent());	

}

// Called when the game starts or when spawned
void AVRCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (BlinkerMaterialBase != nullptr){

		BlinkerMaterial = UMaterialInstanceDynamic::Create(BlinkerMaterialBase,this);

		BlinkerMaterial->SetScalarParameterValue(TEXT("Radius"), RadiusValue);

		Blinker->AddOrUpdateBlendable(BlinkerMaterial);

	}
	
	
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

	UpdateBlinkers();
	
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

bool AVRCharacter::FindTeleportDestination(TArray<FVector> &OutPath,FVector& OutLocation)
{
	FVector Start = RightController->GetComponentLocation();
	FVector End = RightController->GetForwardVector() * DestinationMarkerSpeed;


	FPredictProjectilePathParams Params = FPredictProjectilePathParams(10.0f,Start,End,2.0f,ECollisionChannel::ECC_Visibility, this);
	Params.bTraceComplex = true;
	FPredictProjectilePathResult Result;
  
	bool bHit = UGameplayStatics::PredictProjectilePath(this,Params,Result);
	
	if (!bHit){
		return false;
	}

	for(int i = 0; i < Result.PathData.Num(); i++){
		OutPath.Add(Result.PathData[i].Location);
	}

	FNavLocation NavLocation;
	bool bNavMesh = UNavigationSystemV1::GetCurrent(GetWorld())->ProjectPointToNavigation(Result.HitResult.Location,NavLocation, TeleportProjectionExtent);

	if (!bNavMesh){
		return false;
	}

	OutLocation = NavLocation.Location;

	return true;

}

void AVRCharacter::UpdateDestinationMarker()
{
	TArray<FVector> Path;
	FVector Location;
	bool result = FindTeleportDestination(Path, Location);
	if (result){
		
		DestinationMarker->SetWorldLocation(Location);
		DestinationMarker->SetHiddenInGame(false);
		DrawTeleportPath(Path);
	}else{
		DestinationMarker->SetHiddenInGame(true);
	}
}

void AVRCharacter::UpdateSpline(const TArray<FVector> &Path)
{
	TeleportPath->ClearSplinePoints(false);

	for (int32 i = 0; i < Path.Num(); ++i)
	{
		FVector LocalPosition = TeleportPath->GetComponentTransform().InverseTransformPosition(Path[i]);
		FSplinePoint Point(i, LocalPosition, ESplinePointType::Curve);
		TeleportPath->AddPoint(Point, false);
	}
}

void AVRCharacter::UpdateBlinkers()
{
	if (RadiusVelocity == nullptr){
		return;
	}

	FVector VelocityVector = this->GetVelocity();
	float VelocityValue = VelocityVector.Size();

	BlinkerMaterial->SetScalarParameterValue(TEXT("Radius"), RadiusVelocity->GetFloatValue(VelocityValue));
}

void AVRCharacter::DrawTeleportPath(const TArray<FVector> &Path)
{
	UpdateSpline(Path);

	for (int32 i = 0; i < Path.Num(); ++i)
	{
		if (TeleportPathMeshPool.Num() <= i)
		{
			UStaticMeshComponent* DynamicMesh = NewObject<UStaticMeshComponent>(this);
			DynamicMesh->AttachToComponent(VRRoot, FAttachmentTransformRules::KeepRelativeTransform);
			DynamicMesh->SetStaticMesh(TeleportMesh);
			DynamicMesh->SetMaterial(0, TeleportMaterial);
			DynamicMesh->RegisterComponent();

			TeleportPathMeshPool.Add(DynamicMesh);
		}

		UStaticMeshComponent* DynamicMesh = TeleportPathMeshPool[i];

		DynamicMesh->SetWorldLocation(Path[i]);
	}

}

void AVRCharacter::BeginTeleport()
{
	DestinationPoint = DestinationMarker->GetComponentLocation();
	//Set Timer for FadeTime seconds
	FTimerHandle FadeTimer;
	GetWorldTimerManager().SetTimer(FadeTimer,this,&AVRCharacter::FinishTeleport,FadeTime);

	//Fade Camera to Black
	StartFade(0,1);

}

void AVRCharacter::FinishTeleport()
{
	
	//Teleport to Location
	SetActorLocation(DestinationPoint + GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	//Fade Back In
	StartFade(1,0);

}

void AVRCharacter::StartFade(float FromAlpha, float ToAlpha)
{
	//Get Player Controller
	APlayerController* Player = Cast<APlayerController>(GetController());
	
	if (Player != nullptr){
		Player->PlayerCameraManager->StartCameraFade(FromAlpha,ToAlpha,FadeTime,FLinearColor::Black,false,true);
	}

}

