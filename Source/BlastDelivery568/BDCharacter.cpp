#include "BDCharacter.h"
#include "BDBomb.h"
#include "BDGameMode.h"
#include "BDVisuals.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ABDCharacter::ABDCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(30.f, 64.f);
    // Query-only sweeps block movement, without creating Chaos contact impulses.
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    bUseControllerRotationYaw = false;
    UCharacterMovementComponent* Move = GetCharacterMovement();
    Move->bOrientRotationToMovement = false;
    Move->bEnablePhysicsInteraction = false;
    Move->PushForceFactor = Move->InitialPushForceFactor = Move->TouchForceFactor = 0.f;
    Move->RepulsionForce = Move->StandingDownwardForceScale = 0.f;
    Move->GravityScale = 1.43f;
    Move->AirControl = 0.f;
    Move->FallingLateralFriction = 0.f;
    Move->BrakingDecelerationFalling = 0.f;
    Move->MaxStepHeight = 12.f;
    Move->PerchRadiusThreshold = 12.f;
    Move->bCanWalkOffLedges = true;
    Move->MaxSimulationTimeStep = 1.f / 60.f;
    Move->MaxSimulationIterations = 12;
    AddPart(TEXT("Body"), FVector(0,0,-4), FVector(.46,.46,.66));
    AddPart(TEXT("Head"), FVector(0,0,39), FVector(.45));
    AddPart(TEXT("LeftArm"), FVector(0,-28,-4), FVector(.2,.2,.5));
    AddPart(TEXT("RightArm"), FVector(0,28,-4), FVector(.2,.2,.5));
    AddPart(TEXT("LeftFoot"), FVector(5,-13,-44), FVector(.24,.22,.34));
    AddPart(TEXT("RightFoot"), FVector(5,13,-44), FVector(.24,.22,.34));
    AddPart(TEXT("LeftEye"), FVector(19,-9,48), FVector(.065), true);
    AddPart(TEXT("RightEye"), FVector(19,9,48), FVector(.065), true);
}

UStaticMeshComponent* ABDCharacter::AddPart(const FName Name, const FVector& Location, const FVector& Scale, bool bEye)
{
    UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
    Part->SetupAttachment(GetCapsuleComponent());
    BDVisuals::Shape(Part, TEXT("Sphere"));
    Part->SetRelativeLocation(Location);
    Part->SetRelativeScale3D(Scale);
    if (bEye) Eyes.Add(Part); else BodyParts.Add(Part);
    return Part;
}

void ABDCharacter::BeginPlay()
{
    Super::BeginPlay();
    SpawnTransform = GetActorTransform();
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    for (auto& Part : BodyParts) BDVisuals::Material(Part, TEXT("Player"));
    for (auto& Eye : Eyes) BDVisuals::Material(Eye, TEXT("Ink"));
}

void ABDCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetActorLocation().Z < -350.f) Respawn();
    // Ground bombs can be nudged, but their query-only collision cannot push a crate.
    if (auto* Mode = GetWorld()->GetAuthGameMode<ABDGameMode>())
        if (IsValid(Mode->ActiveBomb) && !Mode->ActiveBomb->IsHeld())
        {
            const FVector Delta = Mode->ActiveBomb->GetActorLocation() - GetActorLocation();
            if (Delta.Size2D() < 62.f && FMath::Abs(Delta.Z + 42.f) < 40.f && GetVelocity().Size2D() > 10.f)
                Mode->ActiveBomb->Nudge(GetVelocity().GetSafeNormal2D());
        }
}

void ABDCharacter::MoveInRoom(const FVector2D& Input)
{
    const FVector2D Clamped = Input.GetClampedToMaxSize(1.f);
    AddMovementInput(FVector(Clamped.X, Clamped.Y, 0.f), Clamped.Size());
}

void ABDCharacter::FacePoint(const FVector& Point)
{
    FVector Direction = Point - GetActorLocation();
    Direction.Z = 0;
    if (!Direction.IsNearlyZero()) SetActorRotation(Direction.Rotation());
}

FVector ABDCharacter::GetBombPosition() const
{
    return GetActorLocation() + GetActorForwardVector() * 50.f + FVector(0,0,-8);
}

void ABDCharacter::DrawBomb()
{
    if (auto* Mode = GetWorld()->GetAuthGameMode<ABDGameMode>()) Mode->DrawBomb(this);
}
void ABDCharacter::DropBomb()
{
    if (auto* Mode = GetWorld()->GetAuthGameMode<ABDGameMode>())
        if (IsValid(Mode->ActiveBomb) && Mode->ActiveBomb->IsHeld()) Mode->ActiveBomb->Release(false);
}
void ABDCharacter::ThrowBomb()
{
    if (auto* Mode = GetWorld()->GetAuthGameMode<ABDGameMode>())
        if (IsValid(Mode->ActiveBomb) && Mode->ActiveBomb->IsHeld()) Mode->ActiveBomb->Release(true);
}
void ABDCharacter::Respawn()
{
    // Dropped/thrown bombs keep burning; a held bomb is left at the fall position.
    DropBomb();
    GetCharacterMovement()->StopMovementImmediately();
    SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
    GetCharacterMovement()->SetMovementMode(MOVE_Falling);
}
