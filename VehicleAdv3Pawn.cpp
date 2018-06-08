// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VehicleAdv3Pawn.h"
#include "VehicleAdv3WheelFront.h"
#include "VehicleAdv3WheelRear.h"
#include "VehicleAdv3Hud.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundCue.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "WheeledVehicleMovementComponent4W.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "GameFramework/Controller.h"
#include "UObject/ConstructorHelpers.h"
#include "SimulationData.h"
#include "DrawDebugHelpers.h"
#include "CustomRamp.h"
#include "Kismet/GameplayStatics.h"
#include "Landmark.h"
#include "TestRunData.h"
#include "CopyVehicleData.h"
#include "Goal.h"
#include "VehicleAdv3.h"

// Needed for VR Headset
#if HMD_MODULE_INCLUDED
#include "IHeadMountedDisplay.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#endif // HMD_MODULE_INCLUDED

const FName AVehicleAdv3Pawn::LookUpBinding("LookUp");
const FName AVehicleAdv3Pawn::LookRightBinding("LookRight");
const FName AVehicleAdv3Pawn::EngineAudioRPM("RPM");

#define LOCTEXT_NAMESPACE "VehiclePawn"

AVehicleAdv3Pawn::AVehicleAdv3Pawn()
{
	// Car mesh
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> CarMesh(TEXT("/Game/VehicleAdv/Vehicle/Vehicle_SkelMesh.Vehicle_SkelMesh"));
	GetMesh()->SetSkeletalMesh(CarMesh.Object);
	
	static ConstructorHelpers::FClassFinder<UObject> AnimBPClass(TEXT("/Game/VehicleAdv/Vehicle/VehicleAnimationBlueprint"));
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(AnimBPClass.Class);

	// Setup friction materials
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> SlipperyMat(TEXT("/Game/VehicleAdv/PhysicsMaterials/Slippery.Slippery"));
	SlipperyMaterial = SlipperyMat.Object;
		
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> NonSlipperyMat(TEXT("/Game/VehicleAdv/PhysicsMaterials/NonSlippery.NonSlippery"));
	NonSlipperyMaterial = NonSlipperyMat.Object;

	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> CustomSlipperyMat(TEXT("/Game/VehicleAdv/PhysicsMaterials/CustomSlippery.CustomSlippery"));
	CustomSlipperyMaterial = CustomSlipperyMat.Object;

	UWheeledVehicleMovementComponent4W* Vehicle4W = CastChecked<UWheeledVehicleMovementComponent4W>(GetVehicleMovement());

	check(Vehicle4W->WheelSetups.Num() == 4);

	// Wheels/Tyres
	// Setup the wheels
	Vehicle4W->WheelSetups[0].WheelClass = UVehicleAdv3WheelFront::StaticClass();
	Vehicle4W->WheelSetups[0].BoneName = FName("PhysWheel_FL");
	Vehicle4W->WheelSetups[0].AdditionalOffset = FVector(0.f, -8.f, 0.f);

	Vehicle4W->WheelSetups[1].WheelClass = UVehicleAdv3WheelFront::StaticClass();
	Vehicle4W->WheelSetups[1].BoneName = FName("PhysWheel_FR");
	Vehicle4W->WheelSetups[1].AdditionalOffset = FVector(0.f, 8.f, 0.f);

	Vehicle4W->WheelSetups[2].WheelClass = UVehicleAdv3WheelRear::StaticClass();
	Vehicle4W->WheelSetups[2].BoneName = FName("PhysWheel_BL");
	Vehicle4W->WheelSetups[2].AdditionalOffset = FVector(0.f, -8.f, 0.f);

	Vehicle4W->WheelSetups[3].WheelClass = UVehicleAdv3WheelRear::StaticClass();
	Vehicle4W->WheelSetups[3].BoneName = FName("PhysWheel_BR");
	Vehicle4W->WheelSetups[3].AdditionalOffset = FVector(0.f, 8.f, 0.f);

	// Adjust the tire loading
	Vehicle4W->MinNormalizedTireLoad = 0.0f;
	Vehicle4W->MinNormalizedTireLoadFiltered = 0.2f;
	Vehicle4W->MaxNormalizedTireLoad = 2.0f;
	Vehicle4W->MaxNormalizedTireLoadFiltered = 2.0f;

	// Engine 
	// Torque setup
	Vehicle4W->MaxEngineRPM = 5700.0f;
	Vehicle4W->EngineSetup.TorqueCurve.GetRichCurve()->Reset();
	Vehicle4W->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(0.0f, 400.0f);
	Vehicle4W->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(1890.0f, 500.0f);
	Vehicle4W->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(5730.0f, 400.0f);
 
	// Adjust the steering 
	Vehicle4W->SteeringCurve.GetRichCurve()->Reset();
	Vehicle4W->SteeringCurve.GetRichCurve()->AddKey(0.0f, 1.0f);
	Vehicle4W->SteeringCurve.GetRichCurve()->AddKey(40.0f, 0.7f);
	Vehicle4W->SteeringCurve.GetRichCurve()->AddKey(120.0f, 0.6f);
			
 	// Transmission	
	// We want 4wd
	Vehicle4W->DifferentialSetup.DifferentialType = EVehicleDifferential4W::LimitedSlip_4W;
	
	// Drive the front wheels a little more than the rear
	Vehicle4W->DifferentialSetup.FrontRearSplit = 0.65;

	// Automatic gearbox
	Vehicle4W->TransmissionSetup.bUseGearAutoBox = true;
	Vehicle4W->TransmissionSetup.GearSwitchTime = 0.15f;
	Vehicle4W->TransmissionSetup.GearAutoBoxLatency = 1.0f;

	// Physics settings
	// Adjust the center of mass - the buggy is quite low
	UPrimitiveComponent* UpdatedPrimitive = Cast<UPrimitiveComponent>(Vehicle4W->UpdatedComponent);
	if (UpdatedPrimitive)
	{
		UpdatedPrimitive->BodyInstance.COMNudge = FVector(8.0f, 0.0f, 0.0f);
	}

	// Set the inertia scale. This controls how the mass of the vehicle is distributed.
	Vehicle4W->InertiaTensorScale = FVector(1.0f, 1.333f, 1.2f);

	// Create a spring arm component for our chase camera
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 34.0f));
	SpringArm->SetWorldRotation(FRotator(-20.0f, 0.0f, 0.0f));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 125.0f;
	SpringArm->bEnableCameraLag = false;
	SpringArm->bEnableCameraRotationLag = false;
	SpringArm->bInheritPitch = true;
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritRoll = true;

	// Create the chase camera component 
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->SetRelativeLocation(FVector(-125.0, 0.0f, 0.0f));
	Camera->SetRelativeRotation(FRotator(10.0f, 0.0f, 0.0f));
	Camera->bUsePawnControlRotation = false;
	Camera->FieldOfView = 90.f;

	// Create In-Car camera component 
	InternalCameraOrigin = FVector(-34.0f, -10.0f, 50.0f);
	InternalCameraBase = CreateDefaultSubobject<USceneComponent>(TEXT("InternalCameraBase"));
	InternalCameraBase->SetRelativeLocation(InternalCameraOrigin);
	InternalCameraBase->SetupAttachment(GetMesh());

	InternalCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InternalCamera"));
	InternalCamera->bUsePawnControlRotation = false;
	InternalCamera->FieldOfView = 90.f;
	InternalCamera->SetupAttachment(InternalCameraBase);

	// In car HUD
	// Create text render component for in car speed display
	InCarSpeed = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarSpeed"));
	InCarSpeed->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.1f));
	InCarSpeed->SetRelativeLocation(FVector(35.0f, -6.0f, 20.0f));
	InCarSpeed->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	InCarSpeed->SetupAttachment(GetMesh());

	// Create text render component for in car gear display
	InCarGear = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarGear"));
	InCarGear->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.1f));
	InCarGear->SetRelativeLocation(FVector(35.0f, 5.0f, 20.0f));
	InCarGear->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	InCarGear->SetupAttachment(GetMesh());
	
	// Setup the audio component and allocate it a sound cue
	static ConstructorHelpers::FObjectFinder<USoundCue> SoundCue(TEXT("/Game/VehicleAdv/Sound/Engine_Loop_Cue.Engine_Loop_Cue"));
	EngineSoundComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineSound"));
	EngineSoundComponent->SetSound(SoundCue.Object);
	EngineSoundComponent->SetupAttachment(GetMesh());

	// Colors for the in-car gear display. One for normal one for reverse
	GearDisplayReverseColor = FColor(255, 0, 0, 255);
	GearDisplayColor = FColor(255, 255, 255, 255);

	bIsLowFriction = false;
	bInReverseGear = false;

	/************************************************************************/
	/*						New Code                                        */
	vehicleType = ECarType::ECT_actual;
	doDataGen = true; // TODO get rid of this later

	// add handler for goal overlap
	OnActorBeginOverlap.AddDynamic(this, &AVehicleAdv3Pawn::BeginOverlap);
	/************************************************************************/
}

void AVehicleAdv3Pawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// set up gameplay key bindings
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AVehicleAdv3Pawn::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AVehicleAdv3Pawn::MoveRight);
	PlayerInputComponent->BindAxis(LookUpBinding);
	PlayerInputComponent->BindAxis(LookRightBinding);

	PlayerInputComponent->BindAction("Handbrake", IE_Pressed, this, &AVehicleAdv3Pawn::OnHandbrakePressed);
	PlayerInputComponent->BindAction("Handbrake", IE_Released, this, &AVehicleAdv3Pawn::OnHandbrakeReleased);
	PlayerInputComponent->BindAction("SwitchCamera", IE_Pressed, this, &AVehicleAdv3Pawn::OnToggleCamera);

	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AVehicleAdv3Pawn::OnResetVR); 

	/************************************************************************/
	/*                          New Code                                    */
	//PlayerInputComponent->BindAction("PauseSimulation", IE_Pressed, this, &AVehicleAdv3Pawn::PauseSimulation);

	//PlayerInputComponent->BindAction("InduceError", IE_Pressed, this, &AVehicleAdv3Pawn::InduceDragError); //TODO get rid of this?
	/************************************************************************/
}

void AVehicleAdv3Pawn::MoveForward(float Val)
{
	GetVehicleMovementComponent()->SetThrottleInput(Val);

}

void AVehicleAdv3Pawn::MoveRight(float Val)
{
	GetVehicleMovementComponent()->SetSteeringInput(Val);
}

void AVehicleAdv3Pawn::OnHandbrakePressed()
{
	GetVehicleMovementComponent()->SetHandbrakeInput(true);
}

void AVehicleAdv3Pawn::OnHandbrakeReleased()
{
	GetVehicleMovementComponent()->SetHandbrakeInput(false);
}

void AVehicleAdv3Pawn::OnToggleCamera()
{
	EnableIncarView(!bInCarCameraActive);
}

void AVehicleAdv3Pawn::EnableIncarView(const bool bState)
{
	if (bState != bInCarCameraActive)
	{
		bInCarCameraActive = bState;
		
		if (bState == true)
		{
			OnResetVR();
			Camera->Deactivate();
			InternalCamera->Activate();
		}
		else
		{
			InternalCamera->Deactivate();
			Camera->Activate();
		}
		
		InCarSpeed->SetVisibility(bInCarCameraActive);
		InCarGear->SetVisibility(bInCarCameraActive);
	}
}

void AVehicleAdv3Pawn::Tick(float Delta)
{
	Super::Tick(Delta);

	/************************************************************************/
	/*							New Code                                    */

	//check if at goal


	// more car forward at a steady rate (for primary and simulation)
	//UE_LOG(VehicleRunState, Log, TEXT("Throttle input: %f"), throttleInput + throttleAdjust);
	GetVehicleMovementComponent()->SetThrottleInput(throttleInput + throttleAdjust); 

	// get current location
	FTransform currentTransform = this->GetTransform();
	FVector currentLocation = currentTransform.GetLocation();
	FQuat currentRotation = currentTransform.GetRotation();

	// periodic camera sweep
	bool bCameraErrorFound = false;
	if (AtTickLocation % 400 == 0 && vehicleType != ECarType::ECT_test && vehicleType != ECarType::ECT_target)
	{
		// 'camera' simulation
		FHitResult output;
		FVector sweepStart = currentLocation + FVector(0, 0, 10.f);
		FVector sweepEnd = currentLocation + currentLocation.ForwardVector * 10.f;
		FCollisionObjectQueryParams params = FCollisionObjectQueryParams(ECC_GameTraceChannel1);	
		const FCollisionShape shape = FCollisionShape::MakeSphere(300.f); 
		FCollisionQueryParams CollisionParams;
		this->outputArray.Empty();
		if (GetWorld()->SweepMultiByObjectType(this->outputArray, sweepStart, sweepEnd, currentRotation, params, shape, CollisionParams))
		{
			TArray<ALandmark*> allLandmarksSeen;
			if (bModelready && expectedFuture->bIsReady)
			{
				bool bLandmarksAccessErrorFound = false;

				//NTODO: problematic because 'new TArray' becomes lost balloon (unreferenced allocated memory) <-- RTODO go over this again
				TArray<ALandmark*> landmarks;
				if (expectedFuture->hasLandmarksAtTick(int(AtTickLocation / 400)))
				{ 
					//NTODO:ptrs?
					landmarks = expectedFuture->GetLandmarksAtTick(int(AtTickLocation / 400)); // TODO triggers error now...???
				}
				else
				{ 
					bLandmarksAccessErrorFound = true;
				}
				if (!bLandmarksAccessErrorFound)
				{
					// didn't see expected number of landmarks for this sweep
					if (outputArray.Num() != landmarks.Num())
					{
						bCameraErrorFound = true;
					}
					// loop over trace hits ('seen objects')
					for (int i = 0; i < outputArray.Num(); i++) //idiom for this: for (auto hitresult : outputArray) {...}
					{
						//TWeakObjectPtr<AActor> hit = outputArray[i].GetActor();
						ALandmark* lmhit = Cast<ALandmark>(outputArray[i].GetActor());
						if (lmhit && lmhit->IsValidLowLevelFast()) 
						{
							//FName seenLandmarkName = hit->GetFName();
							allLandmarksSeen.Add(lmhit);

							// don't need to keep looking if camera error found; offload rest of work 
							if (vehicleType == ECarType::ECT_actual && expectedFuture->bIsReady && !bCameraErrorFound)
							{
								// check if this is the landmark we're supposed to see (if this is not a copy)
								bool hitexpected = CheckLandmarkHit(&landmarks, lmhit); 
								if (!hitexpected)
								{
									bCameraErrorFound = true;
								}
							}
						}
					}
				}

				// free up memory
				//delete landmarks; // pretty sure still need this to exist
			}
			// store landmarks seen at index for this tick
			if (vehicleType == ECarType::ECT_prediction)
			{
				this->LandmarksAlongPath.Add(int(AtTickLocation / 400), allLandmarksSeen);
			}
		}
		// add empty array at tick location
		else if (vehicleType == ECarType::ECT_prediction)
		{
			TArray<ALandmark*>* em = new TArray<ALandmark*>();
			this->LandmarksAlongPath.Add(int(AtTickLocation / 400), *em);
		}
	}
	bool bRpmErrorFound = false;
	bool bRotationErrorFound = false;
	bool bLocationErrorFound = false;
	if (vehicleType == ECarType::ECT_actual)
	{
		if (bGenerateDrift)
		{
			GetVehicleMovementComponent()->SetSteeringInput(0.05f + steerAdjust); // generate slight drift right TODO change value?
		}
		else
		{
			GetVehicleMovementComponent()->SetSteeringInput(steerAdjust);
		}
		if (bModelready && expectedFuture->bIsReady)
		{
			TArray<FTransform> expected = expectedFuture->GetPath(); 
			TArray<float> expectedRPM = expectedFuture->GetRMPValues();
			if (AtTickLocation < expectedRPM.Num())
			{
				FTransform expectedTransform = expected[AtTickLocation];
				FVector expectedLocation = expectedTransform.GetLocation();

				// compare location & check for error (accounting for 4m margin of error on gps irl)
				float distance = expectedLocation.Dist2D(expectedLocation, currentLocation);
				if (distance > 800.f && !bLocationErrorFound) // units are in cm, so error is distance > 8m
				{
					bLocationErrorFound = true;
					GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::FColor(255, 25, 0), FString::Printf(TEXT("Location Error Detected.")));
				}
				// compare rotation and check for error
				float rotationDist = currentRotation.AngularDistance(expectedTransform.GetRotation());
				if (rotationDist > 0.2f && !bRotationErrorFound)
				{
					bRotationErrorFound = true;
					GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("Rotation Error Detected.")));
				}
				// TODO determine if 1.0 is a good threshold -- TODO figure out why begin with very different values
				// (NOTE: real like speedometers word by measuring each tire, so when spinning on slippery ground, speed only goes up if all tires are spinning)
				if (!bRpmErrorFound && FGenericPlatformMath::Abs(expectedRPM[AtTickLocation] - this->GetVehicleMovement()->GetEngineRotationSpeed()) > 90.f) // error threshold set empirically 
				{
					GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("RPM Error Detected.")));
					bRpmErrorFound = true;
				}
				if (bCameraErrorFound || bRotationErrorFound || bRpmErrorFound)
				{
					// TODO call asynchronously so as not to hold up tick?
					ErrorTriage(AtTickLocation, bCameraErrorFound, bRotationErrorFound, bRpmErrorFound, bLocationErrorFound);
				}
				AtTickLocation++;

				// TODO only finds error on first iteration...is that desirable?
			}
		}
	}

	// save path data for copies
	//if (vehicleType != ECarType::ECT_actual)
	if (true)
	{
		PathLocations.Add(this->GetTransform()); 
		VelocityAlongPath.Add(this->GetVelocity());
		RPMAlongPath.Add(GetVehicleMovement()->GetEngineRotationSpeed());
	}
	if (vehicleType == ECarType::ECT_prediction || vehicleType == ECarType::ECT_test)
	{
		GetVehicleMovementComponent()->SetSteeringInput(steerAdjust);
		AtTickLocation++;
	}

	// store performance information at intervals for test runs
	if (vehicleType==ECarType::ECT_test && horizon==HORIZON && tickAtHorizon < 0) // TODO gets in here more than once because horizon increments on seconds w/timer, but ticks are much faster
	{
		tickAtHorizon = AtTickLocation;		
	}
	/************************************************************************/

	// Setup the flag to say we are in reverse gear
	bInReverseGear = GetVehicleMovement()->GetCurrentGear() < 0;
	
	// Update physics material
	UpdatePhysicsMaterial();

	// Update the strings used in the hud (incar and onscreen)
	UpdateHUDStrings();

	// Set the string in the incar hud
	SetupInCarHUD();

	bool bHMDActive = false;
#if HMD_MODULE_INCLUDED
	if ((GEngine->HMDDevice.IsValid() == true ) && ( (GEngine->HMDDevice->IsHeadTrackingAllowed() == true) || (GEngine->IsStereoscopic3D() == true)))
	{
		bHMDActive = true;
	}
#endif // HMD_MODULE_INCLUDED
	if( bHMDActive == false )
	{
		if ( (InputComponent) && (bInCarCameraActive == true ))
		{
			FRotator HeadRotation = InternalCamera->RelativeRotation;
			HeadRotation.Pitch += InputComponent->GetAxisValue(LookUpBinding);
			HeadRotation.Yaw += InputComponent->GetAxisValue(LookRightBinding);
			InternalCamera->RelativeRotation = HeadRotation;
		}
	}	

	// Pass the engine RPM to the sound component
	float RPMToAudioScale = 2500.0f / GetVehicleMovement()->GetEngineMaxRotationSpeed();
	EngineSoundComponent->SetFloatParameter(EngineAudioRPM, GetVehicleMovement()->GetEngineRotationSpeed()*RPMToAudioScale);
}

void AVehicleAdv3Pawn::BeginPlay()
{
	Super::BeginPlay();

	bool bWantInCar = false;
	// First disable both speed/gear displays 
	bInCarCameraActive = false;
	InCarSpeed->SetVisibility(bInCarCameraActive);
	InCarGear->SetVisibility(bInCarCameraActive);

	// Enable in car view if HMD is attached
#if HMD_MODULE_INCLUDED
	bWantInCar = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
#endif // HMD_MODULE_INCLUDED

	EnableIncarView(bWantInCar);
	// Start an engine sound playing
	EngineSoundComponent->Play();

	/************************************************************************/
	/*                       New Code                                       */

	horizonCountdown = false;
	throttleInput = DEFAULT_THROTTLE;
	steerInput = DEFAULT_STEER;
	throttleAdjust = 0.f;
	steerAdjust = 0.f;
	bRunDiagnosticTests = false;

	UE_LOG(VehicleRunState, Log, TEXT("Initial throttle input: %f"), throttleInput);
	UE_LOG(VehicleRunState, Log, TEXT("Initial steering input: %f"), steerInput);

	UE_LOG(LogTemp, Warning, TEXT("DEBUG LOGGING"));
	UE_LOG(VehicleRunState, Log, TEXT("Initial steering input: %f"), steerInput);


	// timer for horizon (stops simulation after horizon reached) TODO use longer time for hypothesis cars
	GetWorldTimerManager().SetTimer(HorizonTimerHandle, this, &AVehicleAdv3Pawn::HorizonTimer, 1.0f, true, 0.f);

	// timer for model generation, generates a new model up to HORIZON every HORIZON seconds -- TODO maybe do at different intervals
	if (vehicleType == ECarType::ECT_actual || vehicleType == ECarType::ECT_datagen)
	{
		GetWorldTimerManager().SetTimer(GenerateExpectedTimerHandle, this, &AVehicleAdv3Pawn::RunTestOrExpect, float(HORIZON) * 2.f, true, 0.f);

		// set timer to use to time run
		GetWorldTimerManager().SetTimer(RunTimerHandle, 1000.f, true, 0.f);
	}
	/************************************************************************/
}

void AVehicleAdv3Pawn::OnResetVR()
{
#if HMD_MODULE_INCLUDED
	if (GEngine->HMDDevice.IsValid())
	{
		GEngine->HMDDevice->ResetOrientationAndPosition();
		InternalCamera->SetRelativeLocation(InternalCameraOrigin);
		GetController()->SetControlRotation(FRotator());
	}
#endif // HMD_MODULE_INCLUDED
}


/************************************************************************/
/*                       New Code                                       */

void AVehicleAdv3Pawn::GenerateTargetRun()
{
	// TODO run until goal and store data
}

bool AVehicleAdv3Pawn::CheckLandmarkHit(TArray<ALandmark*>* expectedLandmarks, ALandmark* seenLandmark)
{
	if (!expectedLandmarks)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::FColor(255, 0, 0), FString::Printf(TEXT("Error Checking Landmarks - Array is null")));
		return false;
	}
	if (expectedLandmarks->Contains(seenLandmark))
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::FColor(0, 255, 75), FString::Printf(TEXT("Landmark Seen!")));
		return true;
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::FColor(0, 255, 75), FString::Printf(TEXT("Landmark Miss!")));
		return false;
	}
}



void AVehicleAdv3Pawn::RunTestOrExpect()
{
	if (vehicleType == ECarType::ECT_datagen)
	{
		return;
	}
	if (doDataGen)
	{
		GenerateDataCollectionRun();
	}
	// see if target run needs to be generated *first*
	else if (!targetRunData)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("Generating Target Run."));
		UE_LOG(VehicleRunState, Log, TEXT("Generating Target Run."));
		// generate target run by spawning target vehicle
		GenerateExpected(); // TODO why are getting here?
	}
	else if (bRunDiagnosticTests)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("Error Found; Going to do diagnostic runs."));
		UE_LOG(ErrorCorrection, Log, TEXT("Starting Error Correction Test Runs."));
		GenerateDiagnosticRuns();
	}
	else
	{
		GenerateExpected();
	}
}

void AVehicleAdv3Pawn::GenerateExpected()
{
	GetWorldTimerManager().PauseTimer(RunTimerHandle);

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("Generating Expected"));
	UE_LOG(VehicleRunState, Log, TEXT("Generating Expected"));

	// begin horizon countdown
	bGenExpected = true;
	horizonCountdown = true;

	// remove old path data
	PathLocations.Empty();
	VelocityAlongPath.Empty();

	this->SetActorTickEnabled(false);

	//possessing new pawn: https://answers.unrealengine.com/questions/109205/c-pawn-possession.html
	AController* controller = this->GetController(); 
	this->StoredController = controller;

	UWheeledVehicleMovementComponent* moveComp = GetVehicleMovement();

	FVector linearveloctiy = this->GetMesh()->GetPhysicsLinearVelocity();
	FVector angularvelocity = this->GetMesh()->GetPhysicsAngularVelocity();
	FTransform currentTransform;
	//currentTransform = this->GetTransform();
	currentTransform = this->GetActorTransform();
	float currRPM = moveComp->GetEngineRotationSpeed();
	// NOTE Modern automobile engines are typically operated around 2,000–3,000 rpm (33–50 Hz) when cruising, with a minimum (idle) speed around 750–900 rpm (12.5–15 Hz), and an upper limit anywhere from 4500 to 10,000 rpm (75–166 Hz) for a road car
	this->ResetRPM = currRPM;
	this->ResetVelocityLinear = linearveloctiy;
	this->ResetVelocityAngular = angularvelocity;
	//NTODONE: pass linearveloctiy et al. by value, these balloons get popped at the end of this scope (})
	//this->dataForSpawn = UCopyVehicleData::MAKE(linearveloctiy, angularvelocity, currentTransform, moveComp->GetCurrentGear(), currRPM); // TODO HELP HELP HELP why is this null?
	this->dataForSpawn = NewObject<UCopyVehicleData>();
	dataForSpawn->Initialize(linearveloctiy, angularvelocity, currentTransform, moveComp->GetCurrentGear(), currRPM);

	//UWheeledVehicleMovementComponent4W* Vehicle4W = CastChecked<UWheeledVehicleMovementComponent4W>(GetVehicleMovement()); //TODO do something with this...

	// copy primary vehicle to make temp vehicle
	FActorSpawnParameters params = FActorSpawnParameters();
	// make sure copy will be flagged as copy
	this->vehicleType = ECarType::ECT_prediction;
	// don't copy over altered drag
	float curdrag = moveComp->DragCoefficient;
	RevertDragError();
	params.Template = this;
	AVehicleAdv3Pawn *copy = GetWorld()->SpawnActor<AVehicleAdv3Pawn>(this->GetClass(), params);
	if (!targetRunData)
	{
		copy->vehicleType = ECarType::ECT_target;
		copy->StoredCopy = this;
		// don't want target run to time-out, only stop when goal is reached
		GetWorldTimerManager().PauseTimer(GenerateExpectedTimerHandle);
		GetWorldTimerManager().PauseTimer(HorizonTimerHandle);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("DEBUG Expected"));
	}
	else
	{
		copy->vehicleType = ECarType::ECT_prediction;
	}
	// reset primary state after copying
	this->vehicleType = ECarType::ECT_actual;
	moveComp->DragCoefficient = curdrag;
	moveComp->SetEngineRotationSpeed(currRPM);
	if (targetRunData)
	{
		this->StoredCopy = copy;
	}

	// get all ramps
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACustomRamp::StaticClass(), FoundActors);
	 //turn off collision for ramps (no collide with simulated vehicle)
	for (AActor* ramp : FoundActors)
	{
		//TODO
		copy->MoveIgnoreActorAdd(ramp); // doesn't seem to work
		if (UStaticMeshComponent* mesh = Cast<UStaticMeshComponent>(ramp->GetRootComponent()))
		{
			mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			//if (bLowFrictionOn)
			//{
			//	// no slipping for copy
			//	mesh->SetPhysMaterialOverride(NonSlipperyMaterial);
			//}
		}
	}

	this->GetMesh()->SetAllBodiesSimulatePhysics(false);

	// copy over state to spawned vehicle**
	// get current speed (heading already copied with transform)
	copy->GetMesh()->SetPhysicsLinearVelocity(linearveloctiy);
	copy->GetMesh()->SetPhysicsAngularVelocity(angularvelocity);

	UWheeledVehicleMovementComponent* copyMoveComp = copy->GetVehicleMovement(); // TODO does this change the original?
	copyMoveComp->SetTargetGear(moveComp->GetCurrentGear(), true);

	copyMoveComp->SetEngineRotationSpeed(moveComp->GetEngineRotationSpeed());

	// switch controller to temp vehicle <= controller seems like it might auto-transfer... (hard to tell)
	if (controller)
	{
		controller->UnPossess(); 
		controller->Possess(copy);
	}
	
}

void AVehicleAdv3Pawn::ResumeExpectedSimulation()
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("RESUME FROM EXPECTED"));
	UE_LOG(VehicleRunState, Log, TEXT("Resuming from Expected"));

	// get all ramps
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACustomRamp::StaticClass(), FoundActors);
	// change collision properties of ramps (no collide with simulated vehicle)
	for (AActor* ramp : FoundActors)
	{
		if (UStaticMeshComponent* mesh = Cast<UStaticMeshComponent>(ramp->GetRootComponent()))
		{
			mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				// apply low physics if error is induced & not copy
				//if (bLowFrictionOn)
				//{
				//	// TODO apply slippery mat to ramp pieces
				//	mesh->SetPhysMaterialOverride(CustomSlipperyMaterial);
				//}
		}
		//ramp->GetRootComponent()->SetSimulatePhysics(false); // doesn't even simulate physics >.<
	}

	// save results for model checking
	//UWheeledVehicleMovementComponent* movecomp = this->StoredCopy->GetVehicleMovement(); // TODO stored copy is null B/C this isn't the og car!! its the copy!!
	if (this->StoredCopy->vehicleType == ECarType::ECT_prediction)
	{
		// save results for model checking
		UWheeledVehicleMovementComponent* movecomp = this->StoredCopy->GetVehicleMovement(); // TODO stored copy is null B/C this isn't the og car!! its the copy!!
		this->expectedFuture = USimulationData::MAKE(this->StoredCopy->GetTransform(), movecomp->GetCurrentGear(), this->StoredCopy->PathLocations, this->StoredCopy->VelocityAlongPath, this->StoredCopy->RPMAlongPath, this->StoredCopy->LandmarksAlongPath);
		//this->expectedFuture = NewObject<USimulationData>();
		// TODO use Initailize() or MAKE()???
		//this->expectedFuture->Initialize(this->StoredCopy->GetTransform(), movecomp->GetCurrentGear(), this->StoredCopy->PathLocations, this->StoredCopy->VelocityAlongPath, this->StoredCopy->RPMAlongPath, this->StoredCopy->LandmarksAlongPath);

		bModelready = true;
	}
	
	// clear timer
	bGenExpected = false;
	horizonCountdown = false;
	horizon = HORIZON;

	// resume primary vehicle
	this->SetActorTickEnabled(true);

	// reset player controller back to primary
	AController* controller = this->StoredController;
	// 	Unposses your current pawn(Controller->UnPossess())
	if (controller)
	{
		controller->UnPossess();
		// 	Re-posses main pawn
		controller->Possess(this);
		// restart original pawn
	}
	
	this->GetMesh()->SetAllBodiesSimulatePhysics(true);
	this->GetMesh()->SetPhysicsLinearVelocity(this->ResetVelocityLinear);
	this->GetMesh()->SetAllPhysicsAngularVelocity(ResetVelocityAngular);
	this->GetVehicleMovement()->SetEngineRotationSpeed(this->ResetRPM);
	// destroy temp vehicle
	this->StoredCopy->Destroy(); // TODO look into this more, do I want to destroy this?

	// spawn sphere to show predicted final destination
	DrawDebugSphere(
		GetWorld(),
		this->expectedFuture->GetTransform().GetLocation(), // TODO expectedFuture is null!!
		40.f,
		32,
		FColor(255, 0, 0),
		false,
		(HORIZON * 4.f)
	);

	UE_LOG(VehicleRunState, Log, TEXT("Expected final location: %s"), *this->expectedFuture->GetTransform().GetLocation().ToString());
	// induce drag error
	/*if (this->GetVehicleMovementComponent()->DragCoefficient < 3000.f)
	{
		InduceDragError();
	}*/

	// change friction
	//InduceFrictionError();

	// reset for error detection
	AtTickLocation = 0;

	InduceSteeringError();

	// empty information before next run
	PathLocations.Empty();
	VelocityAlongPath.Empty();
	RPMAlongPath.Empty();

	GetWorldTimerManager().UnPauseTimer(RunTimerHandle);

	bLocationErrorFound = false;
	bRotationErrorFound = false;
}

void AVehicleAdv3Pawn::ResumeTargetRun()
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("RESUME FROM TARGET RUN"));
	UE_LOG(VehicleRunState, Log, TEXT("Resuming from target run."));

	AVehicleAdv3Pawn* realcar = this->StoredCopy;

	// get all ramps
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACustomRamp::StaticClass(), FoundActors);
	// change collision properties of ramps (no collide with simulated vehicle)
	for (AActor* ramp : FoundActors)
	{
		if (UStaticMeshComponent* mesh = Cast<UStaticMeshComponent>(ramp->GetRootComponent()))
		{
			mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			// apply low physics if error is induced & not copy
			//if (bLowFrictionOn)
			//{
			//	// TODO apply slippery mat to ramp pieces
			//	mesh->SetPhysMaterialOverride(CustomSlipperyMaterial);
			//}
		}
		//ramp->GetRootComponent()->SetSimulatePhysics(false); // doesn't even simulate physics >.<
	}

	// clear timer
	bGenExpected = false;
	horizonCountdown = false;
	horizon = HORIZON;

	realcar->targetRunData = this->targetRunData;

	// resume primary vehicle
	realcar->SetActorTickEnabled(true);

	// reset player controller back to primary
	AController* controller = this->StoredController;
	// 	Unposses your current pawn(Controller->UnPossess())
	if (controller)
	{
		controller->UnPossess();
		// 	Re-posses main pawn
		controller->Possess(realcar);
		// restart original pawn
	}	

	realcar->GetMesh()->SetAllBodiesSimulatePhysics(true);
	realcar->GetMesh()->SetPhysicsLinearVelocity(this->ResetVelocityLinear);
	realcar->GetMesh()->SetAllPhysicsAngularVelocity(ResetVelocityAngular);
	realcar->GetVehicleMovement()->SetEngineRotationSpeed(this->ResetRPM);
	// destroy temp vehicle
	this->Destroy();

	// induce drag error
	/*if (this->GetVehicleMovementComponent()->DragCoefficient < 3000.f)
	{
	InduceDragError();
	}*/

	// change friction
	//InduceFrictionError();

	// reset for error detection
	AtTickLocation = 0;

	//InduceSteeringError();

	bLocationErrorFound = false;
	bRotationErrorFound = false;

	realcar->GetWorldTimerManager().UnPauseTimer(realcar->GenerateExpectedTimerHandle);
	realcar->GetWorldTimerManager().UnPauseTimer(realcar->HorizonTimerHandle);
	realcar->GetWorldTimerManager().UnPauseTimer(realcar->RunTimerHandle);

	//realcar->GetWorldTimerManager().SetTimer(GenerateExpectedTimerHandle, this, &AVehicleAdv3Pawn::RunTestOrExpect, float(HORIZON) * 2.f, true, 0.f);
}


void AVehicleAdv3Pawn::GenerateDiagnosticRuns()
{   
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("Generating Test Runs"));
	UE_LOG(ErrorCorrection, Log, TEXT("Generating Diagnostic Run %i"), NUM_TEST_CARS - runCount);

	tickAtHorizon = -1;
	AtTickLocation = 0;

	GetWorldTimerManager().PauseTimer(GenerateExpectedTimerHandle);
	// keep track of when done running tests
	runCount--; // resets to original value every time... why?
	horizonCountdown = true;
	horizon = 2 * HORIZON; // simulate further into the future
	this->SetActorTickEnabled(false);

	AController* controller = this->GetController();
	this->StoredController = controller;
	FActorSpawnParameters params = FActorSpawnParameters();
	UWheeledVehicleMovementComponent* moveComp = GetVehicleMovement();
	this->vehicleType = ECarType::ECT_test;
	float curdrag = moveComp->DragCoefficient;
	RevertDragError();
	FTransform currentTransform = this->GetActorTransform();
	params.Template = this;
	params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FTransform relocateBy =  currentTransform.Inverse() * dataForSpawn->GetStartPosition();
	AVehicleAdv3Pawn *copy = GetWorld()->SpawnActor<AVehicleAdv3Pawn>(this->GetClass(), relocateBy, params);

	copy->vehicleType = ECarType::ECT_test;
	this->vehicleType = ECarType::ECT_actual;
	// reset primary state after copying <== TODO bother with this here or after spawning all copies? 
	moveComp->DragCoefficient = curdrag;
	this->GetMesh()->SetAllBodiesSimulatePhysics(false);
	copy->GetMesh()->SetPhysicsLinearVelocity(dataForSpawn->GetLinearVelocity());
	copy->GetMesh()->SetPhysicsAngularVelocity(dataForSpawn->GetAngularVelocity());
	UWheeledVehicleMovementComponent* copyMoveComp = copy->GetVehicleMovement();
	copyMoveComp->SetTargetGear(moveComp->GetCurrentGear(), true);
	copyMoveComp->SetEngineRotationSpeed(dataForSpawn->GetRpm());
	this->StoredCopy = copy; // TODO make sure copy isn't empty/stored copy is set appropriately

	// adjust throttle and steering (TODO maybe move this to sep function)
	if (errorDiagnosticResults.bTryThrottle)
	{
		// adjust based on if too fast or too slow or don't know
		if (errorDiagnosticResults.nSpeedDiff == -2) // reversed
		{
			copy->throttleAdjust = FMath::RandRange(0.f, 0.02f);
		}
		else if (errorDiagnosticResults.nSpeedDiff == -1) // too slow
		{
			copy->throttleAdjust = FMath::RandRange(0.f, 0.02f);
		}
		else if (errorDiagnosticResults.nSpeedDiff == 1) // too fast
		{
			copy->throttleAdjust = FMath::RandRange(-0.02f, 0.f);
		}
		else
		{
			copy->throttleAdjust = FMath::RandRange(-0.02f, 0.02f);
		}
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Black, FString::Printf(TEXT("TrhottleAdjust %f"), copy->throttleAdjust));

	}
	if (errorDiagnosticResults.bTrySteer)
	{
		if (errorDiagnosticResults.nDrift == RIGHT)
		{
			copy->steerAdjust = FMath::RandRange(-0.08f, 0.f);
		}
		else if (errorDiagnosticResults.nDrift == LEFT)
		{
			copy->steerAdjust = FMath::RandRange(0.f, 0.05f);
		}
		else
		{
			// don't know which way drifting
			copy->steerAdjust = FMath::RandRange(-0.1f, 0.0f); // TODO change these values back
		}
		GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Black, FString::Printf(TEXT("SteerAdjust %f"), copy->steerAdjust));

	}
	// store what change we're trying and in resume, what it's corresponding result is
	//currentRun = UTestRunData::MAKE(steerAdjust, throttleAdjust);
	currentRun = NewObject<UTestRunData>();
	currentRun->Initialize(copy->steerAdjust, copy->throttleAdjust);
	// switch controller to temp vehicle
	if (controller)
	{
		controller->UnPossess(); 
		controller->Possess(copy);
	}
	if (runCount == 0)
	{
		bRunDiagnosticTests = false;
		lowestCost = -1.;
	}
}

void AVehicleAdv3Pawn::ResumeFromDiagnostic()
{
	// reset timer
	horizonCountdown = false; // TODO maybe reset at end?
	horizon = HORIZON;

	// restore steering and throttle to defaults/expected
	throttleInput = DEFAULT_THROTTLE;

	// resume primary vehicle
	this->SetActorTickEnabled(true);

	// reset player controller back to primary
	AController* controller = this->StoredController;
	if (controller)
	{
		// 	Unposses your current pawn(Controller->UnPossess())
		controller->UnPossess(); 
		// 	Re-posses main pawn
		controller->Possess(this);
	}
	// restart original pawn
	this->GetMesh()->SetAllBodiesSimulatePhysics(true);
	this->GetMesh()->SetPhysicsLinearVelocity(this->ResetVelocityLinear);
	this->GetMesh()->SetAllPhysicsAngularVelocity(ResetVelocityAngular);
	this->GetVehicleMovement()->SetEngineRotationSpeed(this->ResetRPM);
	// destroy temp vehicle

	float cost = calculateTestCost();
	UE_LOG(ErrorCorrection, Log, TEXT("Cost for run %i %f"), NUM_TEST_CARS - runCount, cost);

	if (!lowestCost || lowestCost == -1.)
	{
		lowestCost = cost;
		bestRun = currentRun;
	}
	else if (lowestCost > cost)
	{
		lowestCost = cost;
		bestRun = currentRun;
	}
	// on last run, apply input adjustments with best result
	if (runCount == 0)
	{
		throttleAdjust = bestRun->GetThrottleChange();
		steerAdjust = bestRun->GetSteeringChange();

		GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Green, FString::Printf(TEXT("SteerAdjust Selected %f"), steerAdjust));
		GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Green, FString::Printf(TEXT("ThrottleAdjust Selected %f"), throttleAdjust));

		// empty information before next run
		PathLocations.Empty();
		VelocityAlongPath.Empty();
		RPMAlongPath.Empty();
	}
	this->StoredCopy->Destroy();
	

	GetWorldTimerManager().UnPauseTimer(GenerateExpectedTimerHandle);
}

void AVehicleAdv3Pawn::GenerateDataCollectionRun()
{
	// run through range steering and throttle inputs (along and in pairs) and log results
	float throttleArrayinit[] = { -5.0, -4.8, -4.6, -4.4, -4.2, -4.0, -3.8, -3.6, -3.4, -3.2, -3.0, -2.8, -2.6, -2.4, -2.2, -2.0, -1.8, -1.6, -1.4, -1.2, -1.0, -0.8, -0.6, -0.4, -0.2, 0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.2, 2.4, 2.6, 2.8, 3.0, 3.2, 3.4, 3.6, 3.8, 4.0, 4.2, 4.4, 4.6, 4.8, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -5.0, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.8, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.6, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.4, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.2, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -4.0, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.8, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.6, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.4, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.2, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -3.0, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.8, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.6, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.4, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.2, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -2.0, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.8, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.6, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.4, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.2, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.8, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.6, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.4, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, -0.2, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.2, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.4, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.6, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 1.8, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.2, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.4, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.6, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 2.8, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.2, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.4, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.6, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 3.8, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.2, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.4, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.6, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8, 4.8 };
	TArray<float> throttleInputs;
	throttleInputs.Append(throttleArrayinit, ARRAY_COUNT(throttleArrayinit));
	float steeringArrayinit[] = { 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, -2.0, -1.9, -1.8, -1.7, -1.6, -1.5, -1.4, -1.3, -1.2, -1.1, -1.0, -0.9, -0.8, -0.7, -0.6, -0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9 };
	TArray<float> steeringInputs;
	steeringInputs.Append(steeringArrayinit, ARRAY_COUNT(steeringArrayinit));
	if (controlInputIndex > steeringInputs.Num())
	{
		return;
	}
	// spawn new car rigamarol
	GetWorldTimerManager().PauseTimer(RunTimerHandle);
	if (vehicleType == ECarType::ECT_actual)
	{
		GetWorldTimerManager().PauseTimer(GenerateExpectedTimerHandle);
	}

	// begin horizon countdown
	//bGenExpected = true;
	horizonCountdown = true;

	// remove old path data
	PathLocations.Empty();
	VelocityAlongPath.Empty();

	this->SetActorTickEnabled(false);

	//possessing new pawn: https://answers.unrealengine.com/questions/109205/c-pawn-possession.html
	AController* controller = this->GetController();
	this->StoredController = controller;

	UWheeledVehicleMovementComponent* moveComp = GetVehicleMovement();

	FVector linearveloctiy = this->GetMesh()->GetPhysicsLinearVelocity();
	FVector angularvelocity = this->GetMesh()->GetPhysicsAngularVelocity();
	FTransform currentTransform;
	currentTransform = this->GetActorTransform();
	float currRPM = moveComp->GetEngineRotationSpeed();
	this->ResetRPM = currRPM;
	this->ResetVelocityLinear = linearveloctiy;
	this->ResetVelocityAngular = angularvelocity;
	//NTODONE: pass linearveloctiy et al. by value, these balloons get popped at the end of this scope (})
	//this->dataForSpawn = UCopyVehicleData::MAKE(linearveloctiy, angularvelocity, currentTransform, moveComp->GetCurrentGear(), currRPM); // TODO HELP HELP HELP why is this null?
	this->dataForSpawn = NewObject<UCopyVehicleData>();
	dataForSpawn->Initialize(linearveloctiy, angularvelocity, currentTransform, moveComp->GetCurrentGear(), currRPM);

	// copy primary vehicle to make temp vehicle
	FActorSpawnParameters params = FActorSpawnParameters();
	// make sure copy will be flagged as copy
	this->vehicleType = ECarType::ECT_datagen;
	doDataGen = false;
	params.Template = this;
	AVehicleAdv3Pawn *copy = GetWorld()->SpawnActor<AVehicleAdv3Pawn>(this->GetClass(), params);
	copy->vehicleType = ECarType::ECT_datagen;
	copy->doDataGen = false;

	// reset primary state after copying
	this->vehicleType = ECarType::ECT_actual;
	moveComp->DragCoefficient = moveComp->DragCoefficient;
	moveComp->SetEngineRotationSpeed(currRPM);
	this->StoredCopy = copy;

	this->GetMesh()->SetAllBodiesSimulatePhysics(false);

	// copy over state to spawned vehicle**
	// get current speed (heading already copied with transform)
	copy->GetMesh()->SetPhysicsLinearVelocity(linearveloctiy);
	copy->GetMesh()->SetPhysicsAngularVelocity(angularvelocity);

	UWheeledVehicleMovementComponent* copyMoveComp = copy->GetVehicleMovement(); // TODO does this change the original?
	copyMoveComp->SetTargetGear(moveComp->GetCurrentGear(), true);
	copyMoveComp->SetEngineRotationSpeed(moveComp->GetEngineRotationSpeed());

	// switch controller to temp vehicle <= controller seems like it might auto-transfer... (hard to tell)
	if (controller)
	{
		controller->UnPossess();
		controller->Possess(copy);
	}

	// set to next inputs to try
	copy->throttleInput = throttleInputs[controlInputIndex];
	copy->steerInput = steeringInputs[controlInputIndex];

	// log start transform & inputs
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("Throttle: %f, Steer: %f"), copy->throttleInput, copy->steerInput);
	UE_LOG(ErrorCorrection, Log, TEXT("Throttle: %f, Steer: %f"), copy->throttleInput, copy->steerInput);
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("Start Transform: %s"), *this->GetTransform().ToString());
	UE_LOG(ErrorCorrection, Log, TEXT("Start Transform: %s"), *this->GetTransform().ToString());
	controlInputIndex++;

}

void AVehicleAdv3Pawn::ResumeFromDataGen()
{
	// save data and start next run
	// usual resume process
	// clear timer
	bGenExpected = false;
	horizonCountdown = false;
	horizon = HORIZON;

	// resume primary vehicle
	this->SetActorTickEnabled(true);

	// reset player controller back to primary
	AController* controller = this->StoredController;
	// 	Unposses your current pawn(Controller->UnPossess())
	if (controller)
	{
		controller->UnPossess();
		// 	Re-posses main pawn
		controller->Possess(this);
		// restart original pawn
	}

	this->GetMesh()->SetAllBodiesSimulatePhysics(true);
	this->GetMesh()->SetPhysicsLinearVelocity(this->ResetVelocityLinear);
	this->GetMesh()->SetAllPhysicsAngularVelocity(ResetVelocityAngular);
	this->GetVehicleMovement()->SetEngineRotationSpeed(this->ResetRPM);
								 
	GetWorldTimerManager().UnPauseTimer(RunTimerHandle);

	// log end transform
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, TEXT("End Transform: %s"), *this->GetTransform().ToString());
	UE_LOG(ErrorCorrection, Log, TEXT("End Transform: %s"), *this->StoredCopy->GetTransform().ToString());
	this->StoredCopy->Destroy();
	GenerateDataCollectionRun();
}	

float AVehicleAdv3Pawn::calculateTestCost()
{
	// Weight performance at various intervals (horizon & 2xhorizon(end) for now)
	float endWeight = 0.5f;
	float horizonWeight = 1.f;

	// TODO get performance at horizon TODO do we even have all the info we need?
	SCostComponents test;
	SCostComponents expected;
	SCostComponents actual;
	expected.location = this->expectedFuture->GetTransform().GetLocation(); // (note: horizon will still be last in sequence for non-test cars)
	test.location = StoredCopy->PathLocations[StoredCopy->tickAtHorizon].GetLocation();
	actual.location = this->GetActorTransform().GetLocation();
	expected.rotation = expectedFuture->GetTransform().GetRotation();
	test.rotation = StoredCopy->PathLocations[StoredCopy->tickAtHorizon].GetRotation();
	actual.rotation = this->GetTransform().GetRotation();
	expected.rpm = expectedFuture->GetRMPValues().Last();
	test.rpm = StoredCopy->RPMAlongPath[StoredCopy->tickAtHorizon];
	actual.rpm = this->GetVehicleMovementComponent()->GetEngineRotationSpeed();
	float lossHorizon = QuadraticLoss(expected, test, actual);

	// performance at 2x horizon: look at proximity to goal
	FVector endLocationTest = StoredCopy->GetTransform().GetLocation();
	float lossEnd = distanceToGoal(endLocationTest);

	float reg = Regularize(currentRun->GetThrottleChange(), currentRun->GetSteeringChange());
	float goalBonus = 0.f;
	if (currentRun->hitgoal)
	{
		goalBonus = 10.f;
	}
	return endWeight * lossEnd + horizonWeight * lossHorizon + reg - goalBonus;
}

TArray<float>* AVehicleAdv3Pawn::RotationErrorInfo(int index)
{
	TArray<float>* results = new TArray<float>;

	if (!expectedFuture)
	{ 
		return nullptr;
	}
	TArray<FTransform> expected = expectedFuture->GetPath();
	FTransform currentTransform = this->GetTransform();
	FQuat currentRotation = currentTransform.GetRotation();
	FTransform expectedTransform;
	if (AtTickLocation < expected.Num())
	{
		expectedTransform = expected[AtTickLocation];
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Error: trying to access transform outside array bounds."));
		results->Add(0.f);
		results->Add(0.f);
		results->Add(0.f);
		return results;
	}

	float angDist = currentRotation.AngularDistance(expectedTransform.GetRotation());
	results->Add(angDist);

	float currentForwardX = currentRotation.GetForwardVector().X; // X = -0.199136853 ; left X = 0.199502707
	float expectedForwardX = expectedTransform.GetRotation().GetForwardVector().X; // X = -0.000512242317 ; left X = -0.000133275986
	float veering = currentForwardX > expectedForwardX ? veering = LEFT : RIGHT; // TODO make sure this works (just like keep an eye on it)
	results->Add(veering);
	
	float dotprod = FVector::DotProduct(expectedTransform.GetRotation().GetForwardVector(), currentRotation.GetForwardVector());
	results->Add(dotprod);

	return results;
}

TArray<int>* AVehicleAdv3Pawn::CameraErrorInfo(int index)
{
	int missingRight = 0;
	int missingLeft = 0;
	int extraRight = 0;
	int extraLeft = 0;
	//NTODONE: investigate returning this by value
	TArray<ALandmark*> landmarks = expectedFuture->GetLandmarksAtTick(index);
	for (int i = 0; i < outputArray.Num(); i++)
	{
		ALandmark* lmhit = Cast<ALandmark>(outputArray[i].GetActor());
		if (lmhit->IsValidLowLevelFast())
		{
			if (expectedFuture->bIsReady)
			{
				// seeing things we shouldn't
				if (!landmarks.Contains(lmhit))
				{
					UE_LOG(ErrorDetection, Log, TEXT("Landmark hit unexpected: %s"), *lmhit->GetHumanReadableName());
					(lmhit->IsOnLeft()) ? extraLeft++ : extraRight++;
				}
				else
				{
					// remove seen landmark so we can check what's missing at end
					UE_LOG(ErrorDetection, Log, TEXT("Landmark hit expected: %s"), *lmhit->GetHumanReadableName());
					landmarks.Remove(lmhit);
				}
			}
		}
	}
	for (int i = 0; i < landmarks.Num(); i++)
	{
		ALandmark* lm = landmarks[i];
		UE_LOG(ErrorDetection, Log, TEXT("Landmark miss: %s"), *lm->GetHumanReadableName());
		(lm->IsOnLeft()) ? missingLeft++ : missingRight++;
	}
	// free up memory RTODO: cannot delete since no longer pointer; also how does this now affect ballon problem
	//delete landmarks; 

	TArray<int>* results = new TArray<int>;
	results->Add(missingRight);
	results->Add(missingLeft);
	results->Add(extraRight);
	results->Add(extraLeft);
	return results;
}

int AVehicleAdv3Pawn::GetSideOfLine(FVector a, FVector b, FVector m)
{
	return FMath::Sign((b.X - a.X) * (m.Y - a.Y) - (b.Y - a.Y) * (m.X - a.X));
}

int AVehicleAdv3Pawn::GetFastOrSlow(FVector start, FVector goal, FVector expected, FVector m)
{
	float dstartM = FVector::DistSquaredXY(start, m);
	float dstartE = FVector::DistSquaredXY(start, expected);
	float dgoalM = FVector::DistSquaredXY(goal, m);
	float dgoalE = FVector::DistSquaredXY(goal, expected);
	float dstartGoal = FVector::DistSquaredXY(start, goal);

	// if start is closer to goal than m then could be reversed
	if (dstartGoal < FVector::DistSquaredXY(m, goal))
	{
		return -2;
	}
	// if goal is closer to start than m probably too fast
	if (dstartGoal < FVector::DistSquaredXY(m, start))
	{
		return 1;
	}
	// if m is closer to start than expected then too slow
	if (dstartM < dstartE)
	{
		return -1;
	}
	// if m is closer to goal than expected then too fast
	if (dgoalM < dgoalE)
	{
		return 1;
	}
	return 0;
}

void AVehicleAdv3Pawn::ErrorTriage(int index, bool cameraError, bool headingError, bool rpmError, bool locationError)
{
	/* decide if likely throttle fixable or steering fixable
	 * TODO maybe make a guess if self or environment?
	 * we have (1) landmarks (2) rpm (3) heading (4) rough positioning with "gps"
	 *
	 * save the conclusions to use LATER in generate test in errorDiagnosticResults */

	// make sure only doing this for actual car and that an error was detected
	if (this->vehicleType != ECarType::ECT_actual || bRunDiagnosticTests)
	{
		return;
	}
	bRunDiagnosticTests = true;
	runCount = NUM_TEST_CARS;

	// clear errorDiagnosticResults to make sure it doesn't carry over information
	errorDiagnosticResults.Reset();

	if (cameraError)
	{
		// check seen object differences
		TArray<int>* camerainfo = CameraErrorInfo(index);

		if (camerainfo->operator[](0) < camerainfo->operator[](1) || camerainfo->operator[](2) < camerainfo->operator[](3))
		{
			errorDiagnosticResults.nDrift = RIGHT;
		}
		if (camerainfo->operator[](0) > camerainfo->operator[](1) || camerainfo->operator[](2) > camerainfo->operator[](3))
		{
			errorDiagnosticResults.nDrift = LEFT;
		}
		errorDiagnosticResults.bTryThrottle = true;
		errorDiagnosticResults.bTrySteer = true;

		// make sure memory is freed
		delete camerainfo;
	}
	if (headingError)
	{
		// TODO do more with angle difference etc.
		errorDiagnosticResults.bTrySteer = true;

		// check how heading differs
		/*TArray<float>* rotationInfo = RotationErrorInfo(index);
		float angDist = rotationInfo->operator[](0);
		float veering = rotationInfo->operator[](1);
		float dotprod = rotationInfo->operator[](2);
		// free up memory
		delete rotationInfo;*/
	}
	if (locationError)
	{
		errorDiagnosticResults.bTryThrottle = true;
		errorDiagnosticResults.bTrySteer = true;
		// check if left/right drift
		// TODO what to do if drift already defined and disagree...
		errorDiagnosticResults.nDrift = GetSideOfLine(dataForSpawn->GetStartPosition().GetLocation(), expectedFuture->GetPath()[AtTickLocation].GetLocation(), this->GetActorLocation());
		// check too fast/slow
		errorDiagnosticResults.nSpeedDiff = GetFastOrSlow(dataForSpawn->GetStartPosition().GetLocation(), expectedFuture->GetTransform().GetLocation(), expectedFuture->GetPath()[AtTickLocation].GetLocation(), this->GetActorLocation());
	}
	if (rpmError)
	{
		errorDiagnosticResults.bTryThrottle = true;
		errorDiagnosticResults.nSpeedDiff = GetFastOrSlow(dataForSpawn->GetStartPosition().GetLocation(), expectedFuture->GetTransform().GetLocation(), expectedFuture->GetPath()[AtTickLocation].GetLocation(), this->GetActorLocation());
		errorDiagnosticResults.nDrift = GetSideOfLine(dataForSpawn->GetStartPosition().GetLocation(), expectedFuture->GetPath()[AtTickLocation].GetLocation(), this->GetActorLocation());
	}
}


float AVehicleAdv3Pawn::QuadraticLoss(SCostComponents expected, SCostComponents test, SCostComponents actual)
{
	float total = 0;
	// diff in location
	total += FMath::Pow(FVector::DistSquaredXY(test.location + actual.location, expected.location) , 2);
	// diff in rotation
	total += FMath::Pow(test.rotation.AngularDistance(expected.rotation + actual.rotation), 2);
	// diff in rpm
	total += FMath::Pow((test.rpm + actual.rpm) - expected.rpm, 2);
	return total;
}

float AVehicleAdv3Pawn::Hausdorff(TArray<FTransform> set1, TArray<FTransform> set2, bool rotation)
{
	// compute and return Hausdorff dist
	// based off pseudocode from http://cgm.cs.mcgill.ca/~godfried/teaching/cg-projects/98/normand/main.html
	
	float h = 0.f;
	for (FTransform a : set1)
	{
		float shortest = TNumericLimits< float >::Max();
		for (FTransform b : set2)
		{
			float dist = 0.f;
			if (rotation)
			{
				FQuat rotA = a.GetRotation();
				FQuat rotB = b.GetRotation();
				dist = rotA.AngularDistance(rotB);
			}
			else
			{
				FVector locA = a.GetLocation();
				FVector locB = b.GetLocation();
				dist = locA.DistSquaredXY(locA, locB);
			}
			if (dist < shortest)
			{
				shortest = dist;
			}
		}
		if (shortest > h)
		{
			h = shortest;
		}
	}
	return h;
}

void AVehicleAdv3Pawn::CalculateTotalRunCost()
{
	// TODO calculate total run cost
	float total = 0;

	// get run time difference
	float runtime = GetWorldTimerManager().GetTimerElapsed(RunTimerHandle);
	GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::FColor(55, 25, 20), FString::Printf(TEXT("Run Time; %f"), runtime));

	GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::FColor(55, 25, 20), FString::Printf(TEXT("Run Time Expected; %f"), targetRunData->GetRunTime()));
	UE_LOG(VehicleRunState, Log, TEXT("Run Time; %f"), runtime);
	UE_LOG(VehicleRunState, Log, TEXT("Run Time Expected; %f"), targetRunData->GetRunTime());

	total += FMath::Pow(targetRunData->GetRunTime() - runtime, 2.f);

	// compare paths
	float hdist = Hausdorff(targetRunData->GetPath(), PathLocations, false); // TODO make sure path locations are what we want
	total += FMath::Pow(hdist, 2);

	// compare path rotations TODO does this even make sense to do? kind of, since if the car never points backwards in target but does in 'real' then that's probably not good <-- maybe weigth this less?
	float hdistrot = Hausdorff(targetRunData->GetPath(), PathLocations, true); // TODO make sure path locations are what we want
	total += FMath::Pow(hdistrot, 2);

	// log cost
	UE_LOG(VehicleRunState, Log, TEXT("Total run cost: %f"), total);
	// stop car
	throttleInput = 0.f;
}

float AVehicleAdv3Pawn::Regularize(float deltaThrottle, float deltaSteer)
{
	// TODO adjust lambda
	float lambda = 0.1f;
	return lambda * FMath::Abs(deltaThrottle) + FMath::Abs(deltaSteer);
}

float AVehicleAdv3Pawn::distanceToGoal(FVector objLocation)
{
	// get goal(s) from scene
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGoal::StaticClass(), FoundActors);
	//turn off collision for ramps (no collide with simulated vehicle)
	for (AActor* goal : FoundActors)
	{
		return objLocation.DistSquaredXY(objLocation, goal->GetTransform().GetLocation());
	}
	// can't find goal
	return TNumericLimits< float >::Max();
}

void AVehicleAdv3Pawn::InduceDragError()
{
	UWheeledVehicleMovementComponent* moveComp = GetVehicleMovement();
	float dragCo = moveComp->DragCoefficient;
	moveComp->DragCoefficient = 10.0 * dragCo;
}

void AVehicleAdv3Pawn::RevertDragError()
{
	UWheeledVehicleMovementComponent* moveComp = GetVehicleMovement();
	moveComp->DragCoefficient = 0.3f;
}

void AVehicleAdv3Pawn::InduceSteeringError()
{
	bGenerateDrift = true;
}

void AVehicleAdv3Pawn::StopInduceSteeringError()
{
	bGenerateDrift = false;

}

void AVehicleAdv3Pawn::InduceFrictionError()
{
	GetMesh()->SetPhysMaterialOverride(CustomSlipperyMaterial);
	
	bLowFrictionOn = true;
}

void AVehicleAdv3Pawn::BeginOverlap(class AActor* ThisActor, class AActor* OtherActor)
{
	// check if overlap with goal
	if (Cast<AGoal>(OtherActor) != nullptr)
	{
		
		if (vehicleType == ECarType::ECT_target)
		{
			// store info from target run
			targetRunData = NewObject<USimulationData>();
			targetRunData->InitializeTarget(this->GetTransform(), PathLocations, VelocityAlongPath, RPMAlongPath, this->GetGameTimeSinceCreation());
			UE_LOG(VehicleRunState, Log, TEXT("Target Run Completed")); // TODO add log class for target etc.

			// TODO stop and start real run (transfer controller, etc.) <-- can use ResumeExpected, just don't store expected future
			ResumeTargetRun();
		}
		if (vehicleType == ECarType::ECT_actual)
		{
			// stop generate predictions etc.
			GetWorldTimerManager().PauseTimer(GenerateExpectedTimerHandle);
			GetWorldTimerManager().PauseTimer(RunTimerHandle);
			// do cost calcuation
			CalculateTotalRunCost();
		}
		if (vehicleType == ECarType::ECT_test)
		{
			currentRun->hitgoal = true;
			// TODO figure out if can set timer to 0 and end this test run
			horizon = 0;
		}
	}
}

void AVehicleAdv3Pawn::HorizonTimer()
{
	if (horizonCountdown)
	{
		--horizon;
		if (horizon < 1 && vehicleType != ECarType::ECT_target) // won't resume from target run until goal is reached
		{
			if (StoredCopy->vehicleType == ECarType::ECT_datagen)
			{
				ResumeFromDataGen();
			}
			else if (bGenExpected)
			{
				ResumeExpectedSimulation();
			}
			else
			{
				ResumeFromDiagnostic();
			}
		}
	}
}


/************************************************************************/

void AVehicleAdv3Pawn::UpdateHUDStrings()
{
	float KPH = FMath::Abs(GetVehicleMovement()->GetForwardSpeed()) * 0.036f;
	int32 KPH_int = FMath::FloorToInt(KPH);
	int32 Gear = GetVehicleMovement()->GetCurrentGear();

	// Using FText because this is display text that should be localizable
	SpeedDisplayString = FText::Format(LOCTEXT("SpeedFormat", "{0} km/h"), FText::AsNumber(KPH_int));


	if (bInReverseGear == true)
	{
		GearDisplayString = FText(LOCTEXT("ReverseGear", "R"));
	}
	else
	{
		GearDisplayString = (Gear == 0) ? LOCTEXT("N", "N") : FText::AsNumber(Gear);
	}

}

void AVehicleAdv3Pawn::SetupInCarHUD()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if ((PlayerController != nullptr) && (InCarSpeed != nullptr) && (InCarGear != nullptr))
	{
		// Setup the text render component strings
		InCarSpeed->SetText(SpeedDisplayString);
		InCarGear->SetText(GearDisplayString);
		
		if (bInReverseGear == false)
		{
			InCarGear->SetTextRenderColor(GearDisplayColor);
		}
		else
		{
			InCarGear->SetTextRenderColor(GearDisplayReverseColor);
		}
	}
}

void AVehicleAdv3Pawn::UpdatePhysicsMaterial()
{
	if (GetActorUpVector().Z < 0)
	{
		if (bIsLowFriction == true)
		{
			GetMesh()->SetPhysMaterialOverride(NonSlipperyMaterial);
			bIsLowFriction = false;
		}
		else
		{
			GetMesh()->SetPhysMaterialOverride(SlipperyMaterial);
			bIsLowFriction = true;
		}
	}
}

#undef LOCTEXT_NAMESPACE
